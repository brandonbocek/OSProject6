#ifndef PROJECT6_H
#define PROJECT6_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define NANOPERSECOND 1000000000
#define USER_TERMINATE_BOUND 250000000
#define MAX_TIME_RUN_MASTER 20000000000
#define LINE_LIMIT 1000000
#define RSRC_ARR_SIZE 20
#define PCB_ARRAY_SIZE 18
#define MIN_FUTURE_SPAWN 1000000
#define MAX_FUTURE_SPAWN 500000000

typedef struct clockStruct {
	long long virtualClock;
	int sigNotReceived;
	pid_t scheduledProcess;
} clockStruct;

typedef struct msgbuf {
	long mType;
	char mText[80];
} msgbuf;

typedef struct frame {
	//int quantity[RSRC_ARR_SIZE];
	//int address[256]
	int dirtyBit;
	int logicalBit;
	int referenceBit;
} frame;

typedef struct PCB {
	pid_t processID;
	int request;
	int release;
	int deadlocked;
	int setToDie;
	int pageAddresses[32];
} PCB;

typedef struct resource {
	int quantity;
	int quantAvail;
} resource;

// OSS functions
void printHelpMenu();
void forkAndExecuteNewChild();
int getMessage();
void sendMessage(int, int);
void processMessage(int);
void requestResource(int, int);
void releaseResource(int, int);
void performProcessCleanup(int);
int deadlockIsFound();
int processIsRuledSafe(int*, int);
void outputDeadlockStatus(int *safeProcessArr, int numDeadlocked);
void killAfterDeadlock();
void processResourceRequests();
void interruptHandler(int);
void finalDeletions();
int detachAndRemoveTimer(int, clockStruct*);
int detachAndRemoveArray(int, PCB*);
int detachAndRemoveFrames(int, frame*);

// User functions
int processWillEnd();
void sendMessage(int, int);
void alarmHandler(int);
void signalHandler(int);
void killLeftoverProcesses(int);
 
// Both OSS and User arrays
PCB *pcbGroup;
frame *frameArray;

// Clock variables
struct clockStruct *mainStruct;
long long *virtualClock;
long long CLOCK_INCREMENT_MAX = 10001;

// OSS variables
pid_t myPid, childPid;
FILE *file;
char *filename = "log.out";
int maxTimeToRun = 20;
int verboseOn = 0;
int status;
int shmid;
int pcbShmid;
int frameShmid;
int clockShmid;
int slaveQueueId;
int masterQueueId;
int messageReceived = 0;
int processNumberBeingSpawned = -1;
int totalProcessesSpawned = 0;
int totalGrantedRequests = 0;
int numberOfDeadlockDetectionRuns = 0;
long long fileLinesPrinted = 0;
long long lastDeadlockCheck = 0;
long long timeToSpawn = 0;	//First child spawns at time 0
long long idleTime = 0;
long long turnaroundTime = 0;
long long processWaitTime = 0;
long long totalProcessLifeTime = 0;

// User variables
int processNumber = 0;
int timeoutValue;
int bound;

key_t timerKey = 5745774;
key_t frameArrKey = 9475843;
key_t pcbArrKey = 2143324;
key_t masterQKey = 5489589;

#endif