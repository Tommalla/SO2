/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#ifndef COMMON_H
#define COMMON_H
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>


#define BUF_SIZE 1024
#define KEY_IN 1234L
#define KEY_OUT 1337L
#define KEY_RET 1666L


#ifdef DEBUG
static const unsigned char debug = 1;
#else
static const unsigned char debug = 0;
#endif


struct Message {
	long mtype;     /* type of communicate (>0) */
	char mtext[BUF_SIZE];  /* data */
};


struct ClientInfo {
	pid_t pid;
	int k, n;
};


/**
 * Returns 1 if c is a digit and 0 otherwise
 */
int isDigit(const char c);


/**
 * Returns -1 if str is not an unsigned number, otherwise returns its' integer value
 */
int toUnsignedNumber(const char* str, const int len);


/**
 * Parses the announcement from the client and populates the res. On any error, it returns -1.
 */
int getClientInfo(const char* txt, const int len, struct ClientInfo* res);


#endif