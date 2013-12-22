/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include "common.h"
#include "err.h"

#define MAX_K 99

typedef unsigned short int Resource;

//common data
//queues
int ipcIn;
int ipcOut;

//mutexes and resources
pthread_mutex_t mutex[MAX_K];
Resource resources[MAX_K];
Resource waitingWith[MAX_K];
pthread_cond_t pairCond[MAX_K];
pthread_cond_t resourceCond[MAX_K];

void* client_handler(void* data) {
	int pid, k, n;
	pid = ((struct ClientInfo*)data)->pid;
	k = ((struct ClientInfo*)data)->k;
	n = ((struct ClientInfo*)data)->n;
	int err;

	free(data);
	printf("Inside the new thread: %d %d %d\n", pid, k, n);

	if ((err = pthread_mutex_lock(&mutex[k])) != 0)
		syserr("Error on locking mutex[%d]: %d", k, err);

	if (waitingWith[k]) {	//we have a process to be paired with
		while (waitingWith[k] + n > resources[k])
			if ((err = pthread_cond_wait(&resourceCond[k], &mutex[k])) != 0)
				syserr("Error calling wait on resourceCond[%d]: %d", k, err);

		resources[k] -= n + waitingWith[k];
		waitingWith[k] = 0;

		if ((err = pthread_cond_signal(&pairCond[k])) != 0)
			syserr("Error on signaling pairCond[%d]: %d", k, err);
	} else {
		waitingWith[k] = n;
		if ((err = pthread_cond_wait(&pairCond[k], &mutex[k])) != 0)
			syserr("Error calling wait on pairCond[%d]: %d", k, err);
	}

	if ((err = pthread_mutex_unlock(&mutex[k])) != 0)
		syserr("Error on unlocking mutex[%d]: %d", k, err);

	printf("PID: %d ready!\n", pid);

	//we now have a pair ready to execute

	//get start communication
	struct Message msg;
	msg.mtype = pid;
	sprintf(msg.mtext, "1");

	if (msgsnd(ipcOut, (char*) &msg, 2, 0) != 0)
		syserr("Error while informing the client!");

	if (msgrcv(ipcIn, &msg, BUF_SIZE, pid, 0) == 0)
		syserr("Error: got an empty message from the client!\n");

	//free the resources
	if ((err = pthread_mutex_lock(&mutex[k])) != 0)
		syserr("Error on locking mutex[%d]: %d", k, err);

	printf("Freeing %d of %d after pid %d\n", n, k, pid);
	resources[k] += n;

	if ((err = pthread_mutex_unlock(&mutex[k])) != 0)
		syserr("Error on unlocking mutex[%d]: %d", k, err);

	if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
		syserr("Error on signaling pairCond[%d]: %d", k, err);

	return (void *) 0;
}

struct ClientInfo* info = NULL;

void exit_server(int sig) {
	free(info);
	if (msgctl(ipcIn, IPC_RMID, 0) == -1 || msgctl(ipcOut, IPC_RMID, 0) == -1)
		syserr("Error during msgctl RMID");
	exit(0);
}

int main(const int argc, const char** argv) {
	int k, n, len;
	pthread_t th;
	pthread_attr_t attr;
	int err, i;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Wrong usage! The correct syntax is: ./serwer K N");

	//FIXME delete mutexes and conditions (afterwards)
	for (i = 0; i < k; ++i) {
		resources[i] = n;
		if ((err = pthread_mutex_init(&mutex[i], 0)) != 0)
			syserr("Error on initializing waitingMutex[%d]: %d", i, err);
		if ((err = pthread_cond_init(&pairCond[i], 0)) != 0)
			syserr("Error on initializing pairCond[%d]: %d", i, err);
		if ((err = pthread_cond_init(&resourceCond[i], 0)) != 0)
			syserr("Error on initializing resourceCond[%d]: %d", i, err);
	}

	if (signal(SIGINT,  exit_server) == SIG_ERR)
		syserr("Error during signal");

	//FIXME privileges
	if ((ipcIn = msgget(KEY_IN, 0666 | IPC_CREAT)) == -1)
		syserr("Can't open IPC queue with id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_OUT, 0666 | IPC_CREAT)) == -1)
		syserr("Can't open IPC queue with id %ld", KEY_OUT);

	struct Message msg;

	while (1) {
		if ((len = msgrcv(ipcIn, &msg, BUF_SIZE, 1L, 0)) == 0) {
			printf("Warning: got an empty message from a client. Ignoring...\n");
			continue;
		}

		info = (struct ClientInfo*) malloc(sizeof(struct ClientInfo));
		if (getClientInfo(msg.mtext, strlen(msg.mtext), info) == -1) {
			printf("Warning: got an incorrect message from a client: '%s'. Ignoring...\n", msg.mtext);
			continue;
		}

		printf("Creating a new thread...\n");

		if ((err = pthread_attr_init(&attr)) != 0 )
			syserr("Error on attrinit: %d", err);

		if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
			syserr("Error on setdetach: %d", err);

		if ((err = pthread_create(&th, &attr, &client_handler, info)) != 0)
			syserr("Error on create: %d", err);

		info = NULL;
	}

	return 0;
}

