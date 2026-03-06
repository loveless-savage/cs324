// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823703346
#define BUFSIZE 8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "sockhelper.h"

int verbose = 0;

void prep_msg(unsigned char* buf, int level, int seed);
void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	// parse arguments
	if (argc != 5) {
		fprintf(stderr,"Usage: ./treasure_hunter [server_domain_name] [port#] [level#] [seed#]\n");
		exit(1);
	}
	char *server_domain_name = argv[1];
	char *port = argv[2];
	int level = atoi(argv[3]);
	if (level<0 || level>4) {
		fprintf(stderr,"argument #3 must be a level number between 0 and 4\n");
		exit(1);
	}
	int seed = atoi(argv[4]);

	// load arguments into message request
	unsigned char buf[BUFSIZE];
	prep_msg(buf,level,seed);
	printf("memory copied\n");
	fflush(stdout);
	print_bytes(buf,BUFSIZE);
}

void prep_msg(unsigned char* buf, int level, int seed) {
	bzero(buf,BUFSIZE);

	buf[1] = level;

	unsigned int userid = (unsigned int)htonl(USERID);
	memcpy(&buf[2],&userid,sizeof(unsigned int));

	unsigned short x = htons(seed);
	memcpy(&buf[6],&x,sizeof(unsigned short));
}

void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
	fflush(stdout);
}
