/*
Brandon Bocek
12/1/2017
CS 4760
Project 6
*/

#include "project6.h"

long long lastTimeChecked = 0;

int main (int argc, char *argv[]) {

	shmid = 0;
	frameShmid = 0;
	pcbShmid = 0;
	myPid = getpid();

  /* Command Line args from OSS are processed */

    //segment ID for shared seconds
    processNumber = atoi(argv[1]);
	
	//the message queue segment ID
    shmid = atoi(argv[2]);

    //segment ID process control block
    pcbShmid = atoi(argv[3]);

    //resource segment ID
    frameShmid = atoi(argv[4]);
	
	//message queue id
	masterQueueId = atoi(argv[5]);
	
	 //resource segment ID
    timeoutValue = atoi(argv[6]);

	//Try to attach to shared memory
	if((mainStruct = (clockStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: failed to attach memory segment %i for the virtual clock.\n", shmid);
		exit(EXIT_SUCCESS);
	}

	if((pcbGroup = (PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: Failed to attach memory segment %i for the process control block (PCB)\n", pcbShmid);
		exit(EXIT_SUCCESS);
	}

	if((frameArray = (frame *)shmat(frameShmid, NULL, 0)) == (void *) -1) {
		fprintf(stderr, "Error: failed to attach frame %i for the block \n", frameShmid);
		exit(EXIT_SUCCESS);
	}

	/* Setting the random number generator */
	srand(time(NULL) + getpid());

	/* Setting up signal handlers */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, signalHandler);
	signal(SIGALRM, killLeftoverProcesses);

	/* Child terminates at 10 seconds */
	alarm(10);

	int i, notFinished = 1, nextIndexToRequest = 0;;

	do {
  
    /* If this child has not made a request or release than let it into the CS to maybe make one or the other */
	if(pcbGroup[processNumber].request == -1 && pcbGroup[processNumber].release == -1) {
      //Check to see if process will terminate
		if(processWillEnd(processNumber)) {
			notFinished = 0;
      
      //if not get the next page request
		} else {
				
			if(nextIndexToRequest <= 31) {
					
				// Randomly choose a resource to request
				pcbGroup[processNumber].request = nextIndexToRequest;
				nextIndexToRequest++;
				
				// Sending an update to master					
				sendMessage(masterQueueId, 3);
				
			}
				
			// Sending an update to master
			sendMessage(masterQueueId, 3);
		
		}
	}
    
	} while (notFinished && mainStruct->sigNotReceived && !pcbGroup[processNumber].setToDie);

	/* Tell the master to clear this process's PCB vars */
	if(!pcbGroup[processNumber].setToDie) {
		pcbGroup[processNumber].processID = -1;
		sendMessage(masterQueueId, 3);
	}

	if(shmdt(mainStruct) == -1) {
		perror("Error: Child failed to detach shared memory struct");
	}

	if(shmdt(pcbGroup) == -1) {
		perror("Error: Child failed to detach from shared memory array");
	}

	if(shmdt(frameArray) == -1) {
		perror("Error: Child failed to detach from frame array");
	}

	kill(myPid, SIGTERM);
	sleep(1);
	kill(myPid, SIGKILL);
}
/* END MAIN */

/* if between 0 and 250 ms since last check, the process has a 20% chance to terminate */
int processWillEnd(int pcbIndex) {
	
	/*
	int terminateChance;
	if((mainStruct->virtualClock - lastTimeChecked) >= (rand() % USER_TERMINATE_BOUND)) {
		terminateChance = 1 + rand() % 5;
		lastTimeChecked = mainStruct->virtualClock;
		return terminateChance == 1 ? 1 : 0;
	}	
	lastTimeChecked = mainStruct->virtualClock;
	return 0;
	*/
	
	int memoryReferenceLimit = 1000;
	
	if(rand()%2) {
		memoryReferenceLimit += rand() % 100;
	} else {
		memoryReferenceLimit -= rand() % 100;
	}
	
	if(pcbGroup[pcbIndex].numMemoryReferences >= memoryReferenceLimit) {
		return 1;
	}
	
	return 0;
}

/* Kills the process if it hasn't been killed by OSS yet */
void killLeftoverProcesses(int sig) {
	kill(myPid, SIGTERM);
	sleep(1);
	kill(myPid, SIGKILL);
}

/* Handles kill signal sent from OSS */
void signalHandler(int sig) {
	kill(myPid, SIGKILL);
	alarm(3);
}

/* Send a message to the master to tell it what the child wants done to itself */
void sendMessage(int qid, int msgtype) {
	struct msgbuf msg;

	msg.mType = msgtype;
	sprintf(msg.mText, "%d", processNumber);

	if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
		perror("Error: Child msgsnd has failed");
	}
}