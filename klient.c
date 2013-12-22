/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "err.h"


int main(const int argc, const char** argv) {
	int k = 0, n, s;
	int ipcIn, ipcOut;
	pid_t pid = getpid();

	if (argc != 4 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1 ||
		(s = toUnsignedNumber(argv[3], strlen(argv[3]))) == -1)
		syserr("Błędne użycie! Poprawna składnia to: ./klient k n s");

	k--;

	if ((ipcIn = msgget(KEY_OUT, 0666)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_IN);

	if ((ipcOut = msgget(KEY_IN, 0666)) == -1)
		syserr("Nie można otworzyć kolejki IPC o id %ld", KEY_OUT);

	struct Message msg;

	//announce self to the server
	msg.mtype = 1;
	sprintf(msg.mtext, "%ld %d %d", (long)pid, k, n);

	if (debug)
		fprintf(stderr, "Wysyłam wiadomość...\n");
	if (msgsnd(ipcOut, (char*) &msg, strlen(msg.mtext) + 1, 0) != 0)
		syserr("Błąd przy msgsnd!");

	if (debug)
		fprintf(stderr, "Czekam na odpowiedź...\n");
	if (msgrcv(ipcIn, &msg, BUF_SIZE, pid, 0) == 0)
		syserr("Otrzymałem błędną odpowiedź od serwera. Wychodzę...");

	if (debug)
		fprintf(stderr, "Otrzymałem odpowiedź! Wykonuję się...\n");
	//we have the resources
	sleep(s);

	//free the resources
	msg.mtype = pid;
	sprintf(msg.mtext, "1");

	if (msgsnd(ipcOut, (char*) &msg, 2, 0) != 0)
		syserr("Błąd podczas próby zwolnienia zasobów!");

	printf("%d KONIEC\n", pid);

	return 0;
}