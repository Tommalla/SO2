/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "common.h"
#include "err.h"

//TODO some kind of common data
int ipcIn;
int ipcOut;

void client_handler(const int pid) {
	//TODO
	//handling clients
	//threads should hang on semaphores
	//use pthreads mutexes for protection
}

void exit_server(int sig) {
	if (msgctl(ipcIn, IPC_RMID, 0) == -1 || msgctl(ipcOut, IPC_RMID, 0) == -1)
		syserr("Error during msgctl RMID");
	exit(0);
}

int main(const int argc, const char** argv) {
	int k, n;

	if (argc != 3 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1)
		syserr("Wrong usage! The correct syntax is: ./serwer K N");

	if (signal(SIGINT,  exit_server) == SIG_ERR)
		syserr("Error during signal");

	//FIXME privileges
	if ((ipcIn = msgget(KEY_IN, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
		syserr("Can't open IPC queue with id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_OUT, 0666 | IPC_CREAT | IPC_EXCL)) == -1)
		syserr("Can't open IPC queue with id %ld", KEY_OUT);


	while (1) {
		//TODO
		//read from the queue of requests
		//create a new thread to handle client
		//(use pthreads here)
	}

	return 0;
}

