/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include "common.h"
#include "err.h"

//TODO some kind of common data
int ipcIn;
int ipcOut;

void* client_handler(void* data) {
	struct ClientInfo info;
	info.pid = ((struct ClientInfo*)data)->pid;
	info.k = ((struct ClientInfo*)data)->k;
	info.n = ((struct ClientInfo*)data)->n;

	free(data);
	printf("Inside the new thread: %d %d %d\n", info.pid, info.k, info.n);
	//TODO
	//handling clients
	//threads should hang on semaphores
	//use pthreads mutexes for protection

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
	int err;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Wrong usage! The correct syntax is: ./serwer K N");

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
		if (getClientInfo(msg.text, strlen(msg.text), info) == -1) {
			printf("Warning: got an incorrect message from a client: '%s'. Ignoring...\n", msg.text);
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

