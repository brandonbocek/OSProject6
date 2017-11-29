/*
Brandon Bocek
12/1/2017
CS 4760
Project 6
*/

#include "project6.h"

int main (int argc, char *argv[]) {

    int c;

    /* Handling command line arguments w/ ./oss  */
    while ((c = getopt(argc, argv, "hvl:t:")) != -1)
        switch (c) {
        case 'h':
            printHelpMenu();
        case 'v':
			/* turning verbose mode on */
            verboseOn = 1;
			/* With more prints increment clock quicker for run time speed to be similar */
            CLOCK_INCREMENT_MAX = 100000001;
            break;
        case 'l':
			/*  Change the name of the file to write to */
			filename = optarg;
			break;
        case 't':
		/*  Change the time before the master terminates */
            if(isdigit(*optarg)) {
				maxTimeToRun = atoi(optarg);
			} else {
				fprintf(stderr, "'Give a number with -t'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
        case '?':
			/* User entered a valid flag but nothing after it */
            if (optopt == 'l' || optopt == 't') {
                fprintf(stderr, "-%c needs to have an argument!\n", optopt);
            } else {
                fprintf(stderr, "%s is an unrecognized flag\n", argv[optind - 1]);
            }
        default:
            /* User entered an invalid flag */
			printHelpMenu();
        }
		

    /*Creating and attaching shared memory segments for... */
	
	/* Virtual Clock Shared Mem */
    if((shmid = shmget(timerKey, sizeof(clockStruct), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the virtual clock to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((mainStruct = (struct clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i for the vclock has failed.\n", shmid);
        exit(EXIT_FAILURE);
    }
	
	/* Resource Array Shared Mem */
    if((frameShmid = shmget(frameArrKey, (sizeof(*frameArray) * 256), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the frames array to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((frameArray = (struct frame *)shmat(frameShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i has failed.\n", frameShmid);
        exit(EXIT_FAILURE);
    }

    /* PCB Array Shared Mem */
    if((pcbShmid = shmget(pcbArrKey, (sizeof(*pcbGroup) * 18), IPC_CREAT | 0666)) == -1) {
        perror("Error: shmget for attaching the PCB array to shared memory has failed.");
        exit(EXIT_FAILURE);
    }
    if((pcbGroup = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
        fprintf(stderr, "Error: Attaching memory segment %i for the PCB array has failed.\n", pcbShmid);
        exit(EXIT_FAILURE);
    }

    /*Message queue is created for the OSS Master */
    if((masterQueueId = msgget(masterQKey, IPC_CREAT | 0666)) == -1) {
        fprintf(stderr, "Error: message queue creation has failed\n");
        exit(-1);
    }

    /* Interrupt handlers for Alarm and CTRL-C are initialized */
    signal(SIGALRM, interruptHandler);
    signal(SIGINT, interruptHandler);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    /* Alarm to terminate is set to 20 seconds by default or the command line arg */
    alarm(maxTimeToRun);

	/* Setting the random number generator */
	srand(time(NULL) + getpid());
	
	/* The 20 PCBs in the array have their fields initialized to starting defaults*/
    int x, y;
    for (x = 0; x < PCB_ARRAY_SIZE; x++) {
		for(y = 0; y < 32; y++) {
           // pcbGroup[x].allocation.quantity[y] = 0;
		   pcbGroup[x].pageAddresses[y] = rand() % 256;
        }
        pcbGroup[x].processID = 0;
        pcbGroup[x].deadlocked = 0;
        pcbGroup[x].request = -1;
        pcbGroup[x].release = -1;
		pcbGroup[x].setToDie = 0;
    }
	
	//initialize the Main memory frames
	for(x=0; x< 256; x++) {
		frameArray[x].dirtyBit = 0;
	}
	
	
	/* The 20 resources are initialized */
	
	int indexToBeShared, qtyOfResourcesToBeShared;
    /* Each resource in the array has between 1 and 10 resources available */
	/*
    for(x = 0; x < RSRC_ARR_SIZE; x++) {
        resourceArray[x].quantity = 1 + rand() % 10;
        resourceArray[x].quantAvail = resourceArray[x].quantity;
    }
	*/
    /* Between 3 and 5 of the resources are shareable
	so on average 4 of 20 resource or 20% are shareable	*/
	
	/*
    qtyOfResourcesToBeShared = 3 + rand() % 3;

    for(x = 0; x < qtyOfResourcesToBeShared; x++) {
        indexToBeShared = rand() % RSRC_ARR_SIZE;
        resourceArray[indexToBeShared].quantity = 100;
        resourceArray[indexToBeShared].quantAvail = 100;
    }
	*/

    /*  File pointer opens log.out by default */
    file = fopen(filename, "w");
    if(!file) {
        fprintf(stderr, "Error: failed to open the log file\n");
        exit(EXIT_FAILURE);
    }

	
	/* Start the virtual clock and the main loop in master */
    mainStruct->virtualClock = 0;
    mainStruct->sigNotReceived = 1;

    do {

        if(mainStruct->virtualClock >= timeToSpawn) {
            forkAndExecuteNewChild();
			
			/* The time for the next process to spawn is set */
            timeToSpawn = mainStruct->virtualClock + MIN_FUTURE_SPAWN + rand() % MAX_FUTURE_SPAWN;
			if(verboseOn) {
				printf("OSS: will try to spawn a child process at time %llu.%llu\n", timeToSpawn / NANOPERSECOND, timeToSpawn % NANOPERSECOND);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "OSS: will try to spawn a child process at time %llu%.llu\n", timeToSpawn / NANOPERSECOND, timeToSpawn % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
        }
        if(mainStruct->virtualClock - lastDeadlockCheck > NANOPERSECOND) {
            lastDeadlockCheck = mainStruct->virtualClock;
			
			/* If there is deadlock kill the process using the most resources until deadlock ends */
            /*
			if(deadlockIsFound()) {
                do {
                    killAfterDeadlock();
                } while(deadlockIsFound());
            }
			*/
        }

        mainStruct->virtualClock += 1 + rand() % CLOCK_INCREMENT_MAX;

		/* receive message from child */
        processMessage(getMessage());

        mainStruct->virtualClock += 1 + rand() % CLOCK_INCREMENT_MAX;
        processResourceRequests();


    } while (mainStruct->virtualClock < MAX_TIME_RUN_MASTER && mainStruct->sigNotReceived);


    finalDeletions();

    return 0;
}
/* END MAIN */

void printHelpMenu() {
	printf("\n\t\t~~Help Menu~~\n\t-h This Help Menu Printed\n");
	printf("\t-v *turns on verbose to see extra log file messages\n");
	printf("\t-l *log file used*\t\t\t\tie. '-l log.out'\n");
	printf("\t-t *time in seconds the master will terminate*\tie. -t 20\n\n");
}

/* Forking a new process if there is room for it in the PCB array */
void forkAndExecuteNewChild(void) {

    processNumberBeingSpawned = -1;

    int i;
    for(i = 0; i < PCB_ARRAY_SIZE; i++) {
        if(pcbGroup[i].processID == 0) {
            processNumberBeingSpawned = i;
            pcbGroup[i].processID = 1;
            break;
        }
    }

    if(processNumberBeingSpawned == -1) {
        if(verboseOn) {
            printf("The PCB array is full. No process is to be created for this loop.\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "The PCB array is full. No process is to be created for this loop.\n");
				fileLinesPrinted++;
			}
        }
        
    }else {
        if(verboseOn) {
            printf("An open PCB was located. A new process will be spawned in the opening.\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "An open PCB was located. A new process will be spawned in the opening.\n");
				fileLinesPrinted++;
			}
        }
        totalProcessesSpawned = totalProcessesSpawned + 1;
        
		/* Forking a new child process */
        if((childPid = fork()) < 0) {
            perror("Error: some problem occurred in forking new process\n");
        }
		
		if(verboseOn) {
			printf("Process number %d spawned at time %llu.%llu\n", processNumberBeingSpawned, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file,"Process number %d spawned at time %llu.%llu\n", processNumberBeingSpawned, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				fileLinesPrinted++;
			}
		}
		
        /* The fork succeeded */
        if(childPid == 0) {
            if(verboseOn) {
                printf("Total processes spawned: %d\n", totalProcessesSpawned);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Total processes spawned: %d\n", totalProcessesSpawned);
					fileLinesPrinted++;
				}
            }
       
			pcbGroup[processNumberBeingSpawned].processID = getpid();
			
            char arg1[5], arg2[25], arg3[25], arg4[25], arg5[25], arg6[5];

            //process index of pcb array being spawned
			sprintf(arg1, "%i", processNumberBeingSpawned);
			
			//Shared memory segment id
			sprintf(arg2, "%i", shmid);

            //The PCB's segment id
			sprintf(arg3, "%i", pcbShmid);

            //The frame segment's id
			sprintf(arg4, "%i", frameShmid);

            //The Master queue's message ID
			sprintf(arg5,"%d", masterQueueId);
			
			//Max amount of time the child will run
			sprintf(arg6, "%i", maxTimeToRun);

            /* execute the child process with all the arguments */
			execl("./user", "user", arg1, arg2, arg3, arg4, arg5, arg6,(char *) NULL);
        }
    }
}

//Checks message queue and returns the process location of the sender in the array
int getMessage(void) {
    struct msgbuf msg;

    if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
        if(errno != ENOMSG) {
            perror("Error: OSS has failed to receive a message from the queue.");
            return -1;
        }
        if(verboseOn) {
            printf("No message was found in the queue for the OSS\n");
			if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "No message was found in the queue for the OSS\n");
					fileLinesPrinted++;
				}
        }
        return -1;
    }
	
	/* Success: there is a message */
    else {
        int processNum = atoi(msg.mText);
        return processNum;
    }
}

//THIS MAY NEED SOME SERIOUS WORK
void processMessage(int processNum) {
    if(processNum == -1) {
        return;
    }
    int resourceType;
	
	/* The child wants to request a resource so assign a resource if it is available to assign */
    if((resourceType = pcbGroup[processNum].request) >= 0) {
        requestResource(resourceType, processNum);
		
	/* The child wants to release a resource so release it */
	} else if ((resourceType = pcbGroup[processNum].release) >= 0) {
		releaseResource(resourceType, processNum);
		
	/* The process died so clear its fields for a new process to be spawned in its block */
    } else if(pcbGroup[processNum].processID == -1) {
        performProcessCleanup(processNum);
    } else {
        if(verboseOn) {
            printf("There is nothing to do for this message from child\n");
			if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file, "There is nothing to do for this message from child\n");
				fileLinesPrinted++;
			}
        }
    }
}

/* A process requesting a resource will be given 1 of the resource if its available to be given */
void processResourceRequests(void) {
   
    int i, j;
	/* Loop through all processes to see if any want a request, release, or if they died */
    for(i = 0; i < PCB_ARRAY_SIZE; i++) {
        int resourceType = -1;
        int quant;
     
		/* Finding the right process that is requesting a resource */
        //If the request flag is set with the value of a resource type, process the request
        if((resourceType = pcbGroup[i].request) >= 0) {
            if(verboseOn) {
                printf("Master has detected Process P%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
                if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
            /* Assign a resource if available */
            requestResource(resourceType, i);
        }
        /* Finding any process that wants to be released */
        else if((resourceType = pcbGroup[i].release) >= 0) {
			if(verboseOn) {
				printf("Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Master has detected Process P:%d requesting R:%d at time %llu.%03llu\n", i, resourceType, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
					fileLinesPrinted++;
				}
			}
            releaseResource(resourceType, i);
        }
        /* The process is dead and resources will be returned to resource array */
        else if(pcbGroup[i].processID == -1) {
            performProcessCleanup(i);
        }
    }
}


/* A resource is given to a process */
void requestResource(int resourceType, int i) {
    int quant;
	/*
    if((quant = resourceArray[resourceType].quantAvail) > 0) {
		
		totalGrantedRequests++;
		
        if(verboseOn) {
            printf("There are %d out of %d for resource R:%d available\n", quant, resourceArray[resourceType].quantity, resourceType);
            printf("Increased resource R:%d for process P:%d\n", resourceType, i);
            
			if(fileLinesPrinted < LINE_LIMIT-1) {
				fprintf(file,"Process number %d has requested Resource# %d at time llu.%llu\n",i, resourceType,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
				fprintf(file,"Increased resource %d for process %d\n", resourceType, i);
				fileLinesPrinted = fileLinesPrinted + 2;
			}
        }
       
	   /* Process is given 1 share of its requested resource */
        //pcbGroup[i].allocation.quantity[resourceType]++;
		
		/* The amount of this resource available is 1 less after being allocated to the process */
        //resourceArray[resourceType].quantAvail--;
		
        /* Process is no longer requesting the resource */
        //pcbGroup[i].request = -1;
		/*
        if(verboseOn) {
            printf("After processing the request , there are now %d out of %d available of resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
            if(fileLinesPrinted < LINE_LIMIT) {
				fprintf(file,"After processing the request, there are now %d out of %d available of resource R:%d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
				fileLinesPrinted++;
			}
        }
    }
	*/
	
}

/* A resource is released from a process */
void releaseResource(int resourceType, int i) {
    if(verboseOn) {
        printf("Releasing resouce %d from process %d\n", resourceType, i);
		if(fileLinesPrinted < LINE_LIMIT) {
			fprintf(file,"Releasing resouce %d from process %d at time %llu.%llu\n", resourceType, i ,mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			fileLinesPrinted++;
		}
	}
    
	/* Transferring the resource from the PCB to the resource in the resource array */
    //pcbGroup[i].allocation.quantity[resourceType]--;
    //resourceArray[resourceType].quantAvail++;
	
    
	/* The Process is no longer wanting this resoruce to be released */
    pcbGroup[i].release = -1;
}

void performProcessCleanup(int i) {
	
    if(verboseOn) {
        printf("Process %d has completed and will be killed off at time %llu.%llu\n", i, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
		if(fileLinesPrinted < LINE_LIMIT) {
			fprintf(file,"Process %d has completed and will be killed off at time %llu.%llu\n", i, mainStruct->virtualClock / NANOPERSECOND, mainStruct->virtualClock % NANOPERSECOND);
			fileLinesPrinted++;
		}
    }
    
	// Allocations to the dead process are given back to the array of resources
    int j;
    for(j = 0; j < RSRC_ARR_SIZE; j++) {
        //If the quantity is > 0 for that resource, put them back
		/*
        if(pcbGroup[i].allocation.quantity[j] > 0) {
            if(verboseOn) {
                printf("Before the resources were returned there are %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
				if(fileLinesPrinted < LINE_LIMIT) {
					fprintf(file, "Before the resoruces were returned there are %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
					fileLinesPrinted++;
				}
			}
		*/
            /*Get the quantity to put back into the resource array */
            //int returnQuant = pcbGroup[i].allocation.quantity[j];
			
            /*Increase the resource type quantAvail in the resource array */
		/*
            resourceArray[j].quantAvail += returnQuant;
            if(verboseOn) {
				printf("Returning %d of resource %d from process %d\n", returnQuant, j, i);
                printf("There are now %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
               
				if(fileLinesPrinted < LINE_LIMIT - 1) {
					fprintf(file,"Process %d returning %d of resource R:%d due to termination\n", i,returnQuant,j);
					fprintf(file,"There are now %d out of %d for resource R:%d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
					fileLinesPrinted = fileLinesPrinted + 2;
				}
			}
            //Set the quantity of the pcbGroup to zero
            //pcbGroup[i].allocation.quantity[j] = 0;
        }
		*/
    }
    //Fields for the PCB are reset
	pcbGroup[i].processID = 0;
	pcbGroup[i].request = -1;
	pcbGroup[i].release = -1;
}

/* When an interrupt is called everything is terminated and everything in shared mem is cleared */
void interruptHandler(int SIG) {
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);

	// CTRL-C signal given
    if(SIG == SIGINT) {
        fprintf(stderr, "CTRL-C acknowledged, terminating everything\n");
        finalDeletions();
    }

	// alarm signal given
    if(SIG == SIGALRM) {
        fprintf(stderr, "The time limit has been reached, terminating everything\n");
        finalDeletions();
    }
}

/* functions to detach and remove shared memory items */
int detachAndRemoveFrames(int shmid, frame *shmaddr) {
    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

int detachAndRemoveArray(int shmid, PCB *shmaddr) {
    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

int detachAndRemoveTimer(int shmid, clockStruct *shmaddr) {

    int error = 0;
    if(shmdt(shmaddr) == -1) {
        error = errno;
    }
    if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
        error = errno;
    }
    if(!error) {
        return 0;
    }

    return -1;
}

/* All processes and shared memory is detached and removed */
void finalDeletions() {
	
	//Even when verbose is off print the ending message
	printf("Program ending now: the system ran %d deadlock checks for this run\n", numberOfDeadlockDetectionRuns);
	if(fileLinesPrinted < LINE_LIMIT) {
		fprintf(file, "Program ending now: the system ran %d deadlock checks for this run\n", numberOfDeadlockDetectionRuns);
		fileLinesPrinted++;
	}
	signal(SIGQUIT, SIG_IGN);
	mainStruct->sigNotReceived = 0;
	kill(-getpgrp(), SIGQUIT);
	childPid = wait(&status);

    //Detach and remove the shared memory after all child process have died
    if(detachAndRemoveTimer(shmid, mainStruct) == -1) {
        perror("Error: Failed to destroy shared messageQ shared mem segment");
    }
	
    if(detachAndRemoveArray(pcbShmid, pcbGroup) == -1) {
        perror("Error: Failed to destroy shared pcb shared mem segment");
    }
	
    if(detachAndRemoveFrames(frameShmid, frameArray) == -1) {
        perror("Error: Faild to destroy frame shared mem segment");
    }

    //Deleting the master's message queue
    msgctl(masterQueueId, IPC_RMID, NULL);

	// closing the file pointer
    if(fclose(file)) {
        perror("Error: failed to close the file pointer.");
    }

    exit(EXIT_SUCCESS);
}
