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
pthread_cond_t pairCond[MAX_K];
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

void* client_handler(void* data) {
	int pid, k, n;
	pid = ((struct ClientInfo*)data)->pid;
	k = ((struct ClientInfo*)data)->k;
	n = ((struct ClientInfo*)data)->n;
	int err;

	free(data);
	if (debug)
		fprintf(stderr, "Wewnątrz nowego wątku: pid = %d k = %d n = %d\n", pid, k, n);

	lockMutex(k);

	if (waitingWith[k]) {	//we have a process to be paired with
		while (waitingWith[k] + n > resources[k])
			if ((err = pthread_cond_wait(&resourceCond[k], &mutex[k])) != 0)
				syserr("Błąd przy wait na resourceCond[%d]: %d", k, err);

		resources[k] -= n + waitingWith[k];

		printf("Wątek %02x przydziela %d+%d zasobów %d klientom %d %d, pozostało %d zasobów\n",
		       (unsigned)pthread_self(), n, waitingWith[k], k + 1, pid, waitingPID[k], resources[k]);

		waitingWith[k] = 0;

		if ((err = pthread_cond_signal(&pairCond[k])) != 0)
			syserr("Błąd przy signal na pairCond[%d]: %d", k, err);
	} else {
		waitingWith[k] = n;
		waitingPID[k] = pid;
		if ((err = pthread_cond_wait(&pairCond[k], &mutex[k])) != 0)
			syserr("Błąd przy wait na pairCond[%d]: %d", k, err);
	}

	unlockMutex(k);

	if (debug)
		fprintf(stderr, "PID %d: gotowy!\n", pid);

	//we now have a pair ready to execute

	//get start communication
	struct Message msg;
	msg.mtype = pid;
	sprintf(msg.mtext, "1");

	if (msgsnd(ipcOut, (char*) &msg, 2, 0) != 0)
		syserr("Błąd przy informowaniu (zwalnianiu) klienta!");

	if (msgrcv(ipcIn, &msg, BUF_SIZE, pid, 0) == 0)
		syserr("Otrzymano pustą odpowiedź od klienta!\n");

	//free the resources
	lockMutex(k);

	if (debug)
		fprintf(stderr, "Freeing %d of %d after pid %d\n", n, k, pid);
	resources[k] += n;

	unlockMutex(k);

	if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
		syserr("Błąd przy signal na pairCond[%d]: %d", k, err);

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
		if ((err = pthread_cond_init(&pairCond[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu pairCond[%d]: %d", i, err);
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

		if ((err = pthread_create(&th, &attr, &client_handler, info)) != 0)
			syserr("Błąd przy create: %d", err);

		info = NULL;
	}

	return 0;
}

