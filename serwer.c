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
pthread_mutex_t finishMutex;
pthread_cond_t finishCond;

volatile unsigned char interrupted = 0;
unsigned int aliveThreads = 0;

void lockMutex(const int k) {
	int err;
	if ((err = pthread_mutex_lock(&mutex[k])) != 0)
		syserr("Błąd przy zajmowaniu muteksa[%d]: %d", k, err);
}


void lockFinishMutex() {
	int err;
	if ((err = pthread_mutex_lock(&finishMutex)) != 0)
		syserr("Błąd przy zajmowaniu finishMuteksa: %d", err);
}


void unlockFinishMutex() {
	int err;
	if ((err = pthread_mutex_unlock(&finishMutex)) != 0)
		syserr("Błąd przy zwalnianiu finishMuteksa: %d", err);
}


void unlockMutex(const int k) {
	int err;
	if ((err = pthread_mutex_unlock(&mutex[k])) != 0)
		syserr("Błąd przy zwalnianiu muteksa[%d]: %d", k, err);
}


void increaseAlive() {
	lockFinishMutex();
	++aliveThreads;
	unlockFinishMutex();
}


void decreaseAlive() {
	int err;
	if (debug)
		fprintf(stderr, "Usiłuję skończyć wątek\n");
	lockFinishMutex();
	--aliveThreads;
	if (debug)
		fprintf(stderr, "Kończę wątek! Alive = %d\n", aliveThreads);
	if (aliveThreads == 0) {
		unlockFinishMutex();
		if ((err = pthread_cond_signal(&finishCond)) != 0)
			syserr("Błąd przy signal na finishCond: %d", err);
	}
	unlockFinishMutex();
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

	if (interrupted)
		return (void*) 0;

	increaseAlive();

	lockMutex(k);

	if (waitingWith[k]) {	//we have a process to be paired with
		otherN = waitingWith[k];
		otherPID = waitingPID[k];
		waitingWith[k] = 0;

		while (!interrupted && otherN + n > resources[k]) {
			if (debug)
				fprintf(stderr, "PID %d czeka na %d zasobów typu %d\n", pid, waitingWith[k] + n, k);
			if ((err = pthread_cond_wait(&resourceCond[k], &mutex[k])) != 0)
				syserr("Błąd przy wait na resourceCond[%d]: %d", k, err);
			if (otherN + n > resources[k] && !interrupted) { //we might have caught a wakeup that is not enough for us but is ok for somebody else
				unlockMutex(k);
				if ((err = pthread_cond_signal(&resourceCond[k])) != 0)
					syserr("Błąd przy signal na resourceCond[%d]: %d", k, err);
				lockMutex(k);
			}
		}

		if (interrupted) {
			if (debug)
				fprintf(stderr, "int\n");
			unlockMutex(k);
			decreaseAlive();

			return (void*) 0;
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
		decreaseAlive();
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

	if (debug)
		fprintf(stderr, "%d i %d wysłały komunikaty.\n", pid, otherPID);

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

	decreaseAlive();

	return (void *) 0;
}


struct ClientInfo* info = NULL;
int k, n;


//this method should be called once at the end of program to free all used resources
void cleanup() {
	if (debug)
		fprintf(stderr, "Uruchamiam cleanup!\n");
	int i = 0, err;
	free(info);

	if (msgctl(ipcIn, IPC_RMID, 0) == -1 || msgctl(ipcOut, IPC_RMID, 0) == -1)
		syserr("Błąd przy msgctl RMID");

	for (i = 0; i < k; ++i) {
		if ((err = pthread_mutex_destroy(&mutex[i])) != 0)
			syserr("Błąd przy usuwaniu muteksa[%d]: %d\n", i, err);
		if ((err = pthread_cond_destroy(&resourceCond[i])) != 0)
			syserr("Błąd przy usuwaniu resourceCond[%d]: %d\n", i, err);
	}
}


void dummyHandler(int sig) {
	//noop
}


//a thread whose sole purpose is to exist and catch possible signals
void* signalThread(void* data) {
	sigset_t block_mask;
	int sig, err, i;

	if (signal(SIGINT, &dummyHandler) == SIG_ERR)
		syserr("Signal");

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigwait(&block_mask, &sig);	//wait for the signal
	fprintf(stderr, "Dostałem sygnał!\n");
	interrupted = 1;

	struct Message msg;
	msg.mtype = 1L;

	for (i = 0; i < k; ++i)	//wakey!
		if ((err = pthread_cond_broadcast(&resourceCond[i])) != 0)
			syserr("Błąd przy broadcast na resourceCond[%d]: %d", i, err);

	lockFinishMutex();

	if (debug)
		fprintf(stderr, "signalThread wiesza się na cond (alive = %d)\n", aliveThreads);

	while (aliveThreads > 0)
		if ((err = pthread_cond_wait(&finishCond, &finishMutex)) != 0)
			syserr("Błąd przy wait na finishCond: %d", err);

	unlockFinishMutex();

	//wake up main if there is a need to:
	msgsnd(ipcIn, (char*) &msg, 2, IPC_NOWAIT);

	return (void*) 0;
}


void initResources() {
	int err, i;

	if ((ipcIn = msgget(KEY_IN, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_OUT, 0666 | IPC_CREAT)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_OUT);

	if ((err = pthread_mutex_init(&finishMutex, 0)) != 0)
		syserr("Błąd przy inicjowaniu finishMuteksa]: %d", err);
	if ((err = pthread_cond_init(&finishCond, 0)) != 0)
		syserr("Error przy inicjowaniu finishCond: %d", err);

	for (i = 0; i < k; ++i) {
		resources[i] = n;
		if ((err = pthread_mutex_init(&mutex[i], 0)) != 0)
			syserr("Błąd przy inicjowaniu mutex[%d]: %d", i, err);
		if ((err = pthread_cond_init(&resourceCond[i], 0)) != 0)
			syserr("Error przy inicjowaniu resourceCond[%d]: %d", i, err);
	}
}


int main(const int argc, const char** argv) {
	int len = 0;
	pthread_t th;
	pthread_t cl;
	pthread_attr_t attr;
	int err;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Błędne użycie! Poprawna składnia to: ./serwer K N");

	initResources();
	//atexit(cleanup);

	if ((err = pthread_attr_init(&attr)) != 0 )
		syserr("Błąd przy attrinit: %d", err);

	if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0)
		syserr("Błąd przy setdetachstate: %d", err);

	if ((err = pthread_create(&cl, &attr, &signalThread, info)) != 0)
		syserr("Błąd przy create: %d", err);

	if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
		syserr("Błąd przy setdetachstate: %d", err);

	sigset_t set;
	sigfillset(&set);
	//sigaddset(&set, SIGINT);
	sigprocmask(SIG_BLOCK, &set, NULL);

	struct Message msg;

	while (interrupted == 0) {
		if (interrupted == 1 || (len = msgrcv(ipcIn, &msg, BUF_SIZE, 1L, 0)) == 0) {
			if (debug)
				fprintf(stderr, "Ostrzeżenie: otrzymano pustą wiadomość od klienta. Ignoruję...\n");
			continue;
		}

		info = (struct ClientInfo*) malloc(sizeof(struct ClientInfo));
		if (interrupted == 1 || getClientInfo(msg.mtext, strlen(msg.mtext), info) == -1) {
			if (debug)
				fprintf(stderr, "Ostrzeżenie: otrzymano błędną wiadomość od klienta '%s'. Ignoruję...\n", msg.mtext);
			continue;
		}

		if (debug)
			fprintf(stderr, "Tworzę nowy wątek...\n");

		if (interrupted == 1)
			break;

		if ((err = pthread_create(&th, &attr, &clientHandler, info)) != 0)
			syserr("Błąd przy create: %d", err);

		info = NULL;
	}

	if (debug)
		fprintf(stderr, "Main czeka na zakończenie wątków!\n");

	pthread_join(cl, NULL);
	cleanup();

	return 0;
}

