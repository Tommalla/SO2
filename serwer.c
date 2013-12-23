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
pid_t waitingPID[MAX_K];
pthread_cond_t resourceCond[MAX_K];


void lockMutex(const int k) {
	int err;
	if ((err = pthread_mutex_lock(&mutex[k])) != 0)
		syserr("Błąd przy zajmowaniu muteksa[%d]: %d", k, err);
}


void unlockMutex(const int k) {
	int err;
	if ((err = pthread_mutex_unlock(&mutex[k])) != 0)
		syserr("Błąd przy zwalnianiu muteksa[%d]: %d", k, err);
}


void* clientHandler(void* data) {
	int pid, k, n, otherPID, otherN;
	pid = ((struct ClientInfo*)data)->pid;
	k = ((struct ClientInfo*)data)->k;
	n = ((struct ClientInfo*)data)->n;
	int err;

	free(data);
	if (debug)
		fprintf(stderr, "Wewnątrz nowego wątku: pid = %d k = %d n = %d\n", pid, k, n);

	lockMutex(k);

	if (waitingWith[k]) {	//we have a process to be paired with
		otherN = waitingWith[k];
		otherPID = waitingPID[k];
		waitingWith[k] = 0;

		while (otherN + n > resources[k]) {
			if (debug)
				fprintf(stderr, "PID %d czeka na %d zasobów typu %d\n", pid, waitingWith[k] + n, k);
			if ((err = pthread_cond_wait(&resourceCond[k], &mutex[k])) != 0)
				syserr("Błąd przy wait na resourceCond[%d]: %d", k, err);
			if (otherN + n > resources[k]) { //we might have caught a wakeup that is not enough for us but is ok for somebody else
				unlockMutex(k);
				if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
					syserr("Błąd przy signal na resourceCond[%d]: %d", k, err);
				lockMutex(k);
			}

		}

		resources[k] -= n + otherN;

		printf("Wątek %02x przydziela %d+%d zasobów %d klientom %d %d, pozostało %d zasobów\n",
		       (unsigned)pthread_self(), n, otherN, k + 1, pid, otherPID, resources[k]);

		unlockMutex(k);
	} else {
		waitingWith[k] = n;
		waitingPID[k] = pid;
		if (debug)
			fprintf(stderr, "PID %d czeka na partnera typu %d (wychodzę)\n", pid, k);
		unlockMutex(k);
		return (void*) 0;
	}



	if (debug)
		fprintf(stderr, "PID %d, %d: gotowy!\n", pid, otherPID);

	//we now have a pair ready to execute

	//start communication
	struct Message msg;
	msg.mtype = pid;
	sprintf(msg.mtext, "1");

	if (msgsnd(ipcOut, (char*) &msg, 2, 0) != 0)
		syserr("Błąd przy informowaniu (zwalnianiu) klienta!");

	msg.mtype = otherPID;

	if (msgsnd(ipcOut, (char*) &msg, 2, 0) != 0)
		syserr("Błąd przy informowaniu (zwalnianiu) klienta!");

	if (msgrcv(ipcIn, &msg, BUF_SIZE, pid, 0) == 0)
		syserr("Otrzymano pustą odpowiedź od klienta!\n");

	if (msgrcv(ipcIn, &msg, BUF_SIZE, otherPID, 0) == 0)
		syserr("Otrzymano pustą odpowiedź od klienta!\n");

	//free the resources
	lockMutex(k);

	if (debug)
		fprintf(stderr, "Zwalniam %d+%d typu %d po pidach %d %d\n", n, otherN, k, pid, otherPID);
	resources[k] += n + otherN;

	unlockMutex(k);
	if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
		syserr("Błąd przy signal na resourceCond[%d]: %d", k, err);

	return (void *) 0;
}


struct ClientInfo* info = NULL;


void exit_server(int sig) {
	free(info);
	if (msgctl(ipcIn, IPC_RMID, 0) == -1 || msgctl(ipcOut, IPC_RMID, 0) == -1)
		syserr("Błąd przy msgctl RMID");
	exit(0);
}


int main(const int argc, const char** argv) {
	int k = 0, n = 0, len = 0;
	pthread_t th;
	pthread_attr_t attr;
	int err, i;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Błędne użycie! Poprawna składnia to: ./serwer K N");

	//FIXME delete mutexes and conditions (afterwards)
	for (i = 0; i < k; ++i) {
		resources[i] = n;
		if ((err = pthread_mutex_init(&mutex[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu mutex[%d]: %d", i, err);
		if ((err = pthread_cond_init(&resourceCond[i], 0)) != 0)
			syserr("Error przy inicjowaniu resourceCond[%d]: %d", i, err);
	}

	if (signal(SIGINT,  exit_server) == SIG_ERR)
		syserr("Błąd przy wywołaniu signal");

	if ((ipcIn = msgget(KEY_IN, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_OUT, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_OUT);

	struct Message msg;

	while (1) {
		if ((len = msgrcv(ipcIn, &msg, BUF_SIZE, 1L, 0)) == 0) {
			if (debug)
				fprintf(stderr, "Ostrzeżenie: otrzymano pustą wiadomość od klienta. Ignoruję...\n");
			continue;
		}

		info = (struct ClientInfo*) malloc(sizeof(struct ClientInfo));
		if (getClientInfo(msg.mtext, strlen(msg.mtext), info) == -1) {
			if (debug)
				fprintf(stderr, "Ostrzeżenie: otrzymano błędną wiadomość od klienta '%s'. Ignoruję...\n", msg.mtext);
			continue;
		}

		if (debug)
			fprintf(stderr, "Tworzę nowy wątek...\n");

		if ((err = pthread_attr_init(&attr)) != 0 )
			syserr("Błąd przy attrinit: %d", err);

		if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
			syserr("Błąd przy setdetachstate: %d", err);

		if ((err = pthread_create(&th, &attr, &clientHandler, info)) != 0)
			syserr("Błąd przy create: %d", err);

		info = NULL;
	}

	return 0;
}

