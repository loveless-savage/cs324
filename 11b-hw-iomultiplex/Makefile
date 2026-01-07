CC = gcc
CFLAGS = -Wall -O2 -g -I ../include/

.PHONY: all
all: echoservers

echoservers: echoservers.c ../code/sockhelper.c
	$(CC) $(CFLAGS) -o echoservers echoservers.c ../code/sockhelper.c

.PHONY: clean
clean:
	rm -f echoservers
