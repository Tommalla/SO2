/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <sys/ipc.h>
#include <sys/msg.h>

const int BUF_SIZE = 1024;
const long KEY_IN = 1234L;
const long KEY_OUT = 1337L;

/**
 * Returns 1 if c is a digit and 0 otherwise
 */
int isDigit(const char c);

/**
 * Returns -1 if str is not an unsigned number, otherwise returns its' integer value
 */
int toUnsignedNumber(const char* str, const int len);