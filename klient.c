/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "err.h"

int main(const int argc, const char** argv) {
	int k, n, s;
	int ipcIn, ipcOut;

	if (argc != 4 || (k = toUnsignedNumber(argv[1], strlen(argv[1]))) == -1 || (n = toUnsignedNumber(argv[2], strlen(argv[2]))) == -1 ||
		(s = toUnsignedNumber(argv[3], strlen(argv[3]))) == -1)
		syserr("Wrong usage! The correct syntax is: ./klient k n s");
	//TODO:
	//get the queues
	//inform of needs
	//wait in the queue (wait for your id)

	sleep(s);

	//tell server to free resources


	return 0;
}