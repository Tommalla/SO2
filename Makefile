.PHONY: clean

CFLAGS = -Wall -std=c99
IPC_FLAGS = -D_XOPEN_SOURCE=600

ifeq ($(debug),1)
	CFLAGS+= -g -DDEBUG
else
	CFLAGS+= -O2
endif

all: serwer klient

serwer: serwer.c err.o common.o
	gcc $(CFLAGS) $(IPC_FLAGS) -pthread serwer.c err.o common.o -o serwer

klient: klient.c err.o common.o
	gcc $(CFLAGS) $(IPC_FLAGS) klient.c err.o common.o -o klient

err.o: err.c err.h
	gcc $(CFLAGS) -c err.c -o err.o

common.o: common.c common.h
	gcc $(CFLAGS) $(IPC_FLAGS) -c common.c -o common.o

clean:
	rm -rf *.o
