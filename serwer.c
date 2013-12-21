/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdlib.h>
#include "common.h"

//TODO some kind of common data

void client_handler(const int pid) {
	//TODO
	//handling clients
	//threads should hang on semaphores
	//use pthreads mutexes for protection
}

int main(const int argc, const char** argv) {
	//TODO
	//parse arguments
	//infinately:
	//	read from the queue of requests
	//	create a new thread to handle client
	//	(use pthreads here)

	return 0;
}

