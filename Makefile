.PHONY: clean

CFLAGS = -Wall -pedantic -std=c99

all: serwer klient

serwer: serwer.c
	gcc $(CFLAGS) serwer.c -o serwer

klient: klient.c
	gcc $(CFLAGS) klient.c -o klient

clean:
	rm -rf *.o
