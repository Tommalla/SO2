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


//finishing program
unsigned char interrupted = 0, finished = 0;	//when set to 1 cause
unsigned int aliveThreads = 1;	//main thread
pthread_cond_t cleanupCond;
pthread_mutex_t cleanupMutex;
pthread_mutex_t intMutex;


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


void lockIntMutex() {
	int err;
	if ((err = pthread_mutex_lock(&intMutex)) != 0)
		syserr("Błąd przy zajmowaniu intMuteksa: %d", err);
}


void unlockIntMutex() {
	int err;
	if ((err = pthread_mutex_unlock(&intMutex)) != 0)
		syserr("Błąd przy zwalnianiu intMuteksa: %d", err);
}


void signalCleanupCond() {
	if (debug)
		fprintf(stderr, "signalCleanupCond()\n");
	int err;
	if ((err = pthread_cond_signal(&cleanupCond)) != 0)
		syserr("Błąd przy signal na cleanupCond: %d", err);
}


void* client_handler(void* data) {
	lockIntMutex();
	++aliveThreads;
	unlockIntMutex();

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
		while (waitingWith[k] + n > resources[k] && interrupted == 0)
			if ((err = pthread_cond_wait(&resourceCond[k], &mutex[k])) != 0)
				syserr("Błąd przy wait na resourceCond[%d]: %d", k, err);

		if (interrupted == 1) {
			unlockMutex(k);
			lockIntMutex();
			--aliveThreads;
			if (aliveThreads <= 0) {
				unlockIntMutex();
				signalCleanupCond();
			}
			unlockIntMutex();
			return (void*) 0;
		}

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

	lockIntMutex();
	if (interrupted == 0) {
		unlockIntMutex();
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
	}

	unlockIntMutex();

	//free the resources
	lockMutex(k);

	if (debug)
		fprintf(stderr, "Zwalniam %d zasobu %d po pid %d\n", n, k, pid);
	resources[k] += n;
	unlockMutex(k);

	lockIntMutex();
	--aliveThreads;

	//if it's the last thread and the cleanup should happen now
	if (interrupted == 1 && aliveThreads == 0) {
		unlockIntMutex();
		signalCleanupCond();
	} else {
		unlockIntMutex();

		if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
			syserr("Błąd przy signal na resourceCond[%d]: %d", k, err);
	}

	return (void *) 0;
}


struct ClientInfo* info = NULL;
int k, n;


void signalHandler(int sig) {
	if (debug)
		fprintf(stderr, "Signal : %d, obsłużony! %d alive\n", sig, aliveThreads);
	interrupted = 1;
	int err;
	for (int i = 0; i < k; ++i) {
		if ((err = pthread_cond_signal(&resourceCond[i])) != 0)
 			syserr("Błąd przy signal na resourceCond[%d]: %d", i, err);
		if ((err = pthread_cond_signal(&pairCond[i])) != 0)
			syserr("Błąd przy wait na pairCond[%d]: %d", i, err);
	}
}


void initResources() {
	int i, err;

	if ((err = pthread_mutex_init(&intMutex, 0)) != 0)
		syserr("Błąd przy inicjowaniu intMuteksa: %d", err);

	if ((err = pthread_mutex_init(&cleanupMutex, 0)) != 0)
		syserr("Błąd przy inicjowaniu cleanupMuteksa: %d", err);

	if ((err = pthread_cond_init(&cleanupCond, 0)) != 0)
		syserr("Błąd przy inicjowaniu cleanupCond: %d", err);

	for (i = 0; i < k; ++i) {
		resources[i] = n;
		if ((err = pthread_mutex_init(&mutex[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu mutex[%d]: %d", i, err);
		if ((err = pthread_cond_init(&pairCond[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu pairCond[%d]: %d", i, err);
		if ((err = pthread_cond_init(&resourceCond[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu resourceCond[%d]: %d", i, err);
	}
}


void initSignals() {
	struct sigaction sigact;
	sigset_t block_mask;

	sigact.sa_handler = &signalHandler;
	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigact.sa_mask = block_mask;
	sigact.sa_flags = 0;

	if (sigaction(SIGINT, &sigact, 0) == -1)
		syserr("Błąd przy wywołaniu sigaction");
}


//this method should be called once at the end of program to free all used resources
void cleanup() {
	if (debug)
		fprintf(stderr, "Uruchamiam cleanup!\n");
	int i = 0, err;
	free(info);

	if (msgctl(ipcIn, IPC_RMID, 0) == -1 || msgctl(ipcOut, IPC_RMID, 0) == -1)
		syserr("Błąd przy msgctl RMID");

	if ((err = pthread_mutex_destroy(&intMutex)) != 0)
		syserr("Błąd przy usuwaniu intMuteksa: %d\n", err);
	if ((err = pthread_mutex_destroy(&cleanupMutex)) != 0)
		syserr("Błąd przy usuwaniu cleanupMuteksa: %d\n", err);
	if ((err = pthread_cond_destroy(&cleanupCond)) != 0)
		syserr("Błąd przy usuwaniu cleanupCond: %d\n", err);

	for (i = 0; i < k; ++i) {
		if ((err = pthread_mutex_destroy(&mutex[i])) != 0)
			syserr("Błąd przy usuwaniu muteksa[%d]: %d\n", i, err);
		if ((err = pthread_cond_destroy(&pairCond[i])) != 0)
			syserr("Błąd przy usuwaniu pairCond[%d]: %d\n", i, err);
		if ((err = pthread_cond_destroy(&resourceCond[i])) != 0)
			syserr("Błąd przy usuwaniu resourceCond[%d]: %d\n", i, err);
	}

	finished = 1;
}


void* cleanup_thread(void* data) {
	int err;

	lockIntMutex();
	if (interrupted == 1 && aliveThreads == 0)
		cleanup();
	else {
		if (debug)
			fprintf(stderr, "cleanup thread: wieszam się na cond!\n");
		if ((err = pthread_cond_wait(&cleanupCond, &intMutex)) != 0)
			syserr("Błąd przy wait na cleanupCond: %d", k, err);

		unlockIntMutex();

		cleanup();
	}

	return (void*) 0;
}


int main(const int argc, const char** argv) {
	int len = 0;
	pthread_t th;
	pthread_t clnp;
	pthread_attr_t attr;
	int err;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Błędne użycie! Poprawna składnia to: ./serwer K N");

	initSignals();
	initResources();

	if ((ipcIn = msgget(KEY_IN, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_OUT, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_OUT);

	if ((err = pthread_attr_init(&attr)) != 0 )
		syserr("Błąd przy attrinit: %d", err);

	if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
		syserr("Błąd przy setdetachstate: %d", err);

	if ((err = pthread_create(&clnp, &attr, &cleanup_thread, NULL)) != 0)
		syserr("Błąd przy create: %d", err);

	if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
		syserr("Błąd przy setdetachstate: %d", err);

	struct Message msg;

	while (1) {
		lockIntMutex();
		if (interrupted == 1) {
			--aliveThreads;
			if (aliveThreads == 0) {
				unlockIntMutex();
				signalCleanupCond();
			} else {
				//if (debug)
				unlockIntMutex();
			}
			break;
		}
		unlockIntMutex();

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

		lockIntMutex();
		if (interrupted == 1) {
			--aliveThreads;
			if (aliveThreads == 0) {
				unlockIntMutex();
				signalCleanupCond();
			} else {
				//if (debug)
				unlockIntMutex();
			}
			break;
		}
		unlockIntMutex();

		if (debug)
			fprintf(stderr, "Tworzę nowy wątek...\n");

		if ((err = pthread_create(&th, &attr, &client_handler, info)) != 0)
			syserr("Błąd przy create: %d", err);

		info = NULL;
	}

	pthread_join(clnp, NULL);

	return 0;
}

