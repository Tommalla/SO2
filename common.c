/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#include <stdlib.h>
#include "common.h"

int isDigit(const char c) {
	return '0' <= c && c <= '9';
}


int toUnsignedNumber(const char* str, const int len) {
	int i;
	int res = 0;
	for (i = 0; i < len; ++i)
		if (!isDigit(str[i]))
			return -1;
		else {
			res *= 10;
			res += str[i] - '0';
		}

	return res;
}


int getClientInfo(const char* txt, const int len, struct ClientInfo* res) {
	int i = 0, j = 0, id;
	long int resNum[3] = {0, 0, 0};

	if (len < 5)
		return -1;

	for (id = 0; id < 3; ++id) {
		if (!isDigit(txt[i]))
			return -1;

		j = i;
		for (; i < len && isDigit(txt[i]); ++i);

		if ((resNum[id] = toUnsignedNumber(txt + j, i - j)) == -1)
			return -1;
		++i;
	}

	res->pid = resNum[0];
	res->k = resNum[1];
	res->n = resNum[2];

	return 0;
}
