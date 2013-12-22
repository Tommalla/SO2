/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#define BUF_SIZE 1024
#define KEY_IN 1234L
#define KEY_OUT 1337L

struct Message {
	long type;     /* type of communicate (>0) */
	char text[BUF_SIZE];  /* data */
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
 * Parses the announcement from the client and populates ClientInfo. On any error, it returns ClientInfo with
 * k = -1 and n = -1
 */
struct ClientInfo getClientInfo(const char* txt, const int len);