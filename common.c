/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/

int isDigit(const char c) {
	return '0' <= c && c <= '9';
}

int toUnsignedNumber(const char* str, const int len) {
	int i;
	int res = 0;
	for (i = len - 1; i >= 0; --i)
		if (!isDigit(str[i]))
			return -1;
		else {
			res *= 10;
			res += str[i] - '0';
		}

	return res;
}
