// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823703346
#define BUFSIZE 8
#define MAX_RECVSIZE 256
#define MAX_TOT_RECVSIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "sockhelper.h"

int sfd = -1;
struct sockaddr *remote_addr = NULL;
socklen_t addr_len = sizeof(struct sockaddr_storage);
int verbose = 0;

void prep_msg(unsigned char* buf, int level, int seed);
void request(unsigned char *buf, size_t send_size, unsigned char *bufrecv);
void parse_response(unsigned char *bufrecv, unsigned char chunk_len, unsigned char *treasure, unsigned char *op, unsigned short *op_param, unsigned int *nonce);
void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	// parse arguments
	if (argc != 5) {
		fprintf(stderr,"Usage: ./treasure_hunter [server_domain_name] [port#] [level#] [seed#]\n");
		exit(EXIT_FAILURE);
	}
	char *server_domain_name = argv[1];
	char *port = argv[2];
	int level = atoi(argv[3]);
	if (level<0 || level>4) {
		fprintf(stderr,"argument #3 must be a level number between 0 and 4\n");
		exit(EXIT_FAILURE);
	}
	int seed = atoi(argv[4]);

	// load arguments into message request
	unsigned char buf[BUFSIZE];
	prep_msg(buf,level,seed);

	// query remote address info for UDP connection
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	struct addrinfo *result;
	int s = getaddrinfo(server_domain_name,port,&hints,&result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	// parse query response
	struct sockaddr_storage remote_addr_ss;
	remote_addr = (struct sockaddr *)&remote_addr_ss;
	memcpy(remote_addr, result->ai_addr, sizeof(struct sockaddr_storage));
	char remote_ip[INET6_ADDRSTRLEN];
	unsigned short remote_port;
	parse_sockaddr(remote_addr, remote_ip, &remote_port);
	if (verbose) {
		fprintf(stderr, "Connecting to %s:%d\n", remote_ip, remote_port);
	}
	// initialize socket
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd<0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	// capture local address info
	struct sockaddr_storage local_addr_ss;
	struct sockaddr *local_addr = (struct sockaddr *)&local_addr_ss;
	s = getsockname(sfd, local_addr, &addr_len);
	char local_ip[INET6_ADDRSTRLEN];
	unsigned short local_port;
	parse_sockaddr(local_addr, local_ip, &local_port);

	// send message and collect response
	unsigned char bufrecv[MAX_RECVSIZE];
	request(buf,BUFSIZE,bufrecv);
	// parse response
	unsigned char chunk_len = 0;
	unsigned char full_treasure[MAX_TOT_RECVSIZE];
	unsigned char *treasure = full_treasure;
	unsigned char op;
	unsigned short op_param;
	unsigned int nonce, req;
	chunk_len = bufrecv[0];
	if (chunk_len==0) {
		// TODO: treasure hunt over
	} else if (chunk_len>127) {
		// TODO: server error
	}
	parse_response(bufrecv,chunk_len,treasure,&op,&op_param,&nonce);
	if (verbose) {
		printf("chunk length = %d\ttreasure = %s\top = 0x%x\top_param = 0x%x\tnonce = 0x%x\n",
				chunk_len,treasure,op,op_param,nonce);
	}
	treasure += chunk_len;

	// enter request + response loop
	while (1) {
		// follow-up request
		req = (unsigned int)htonl(nonce+1);
		if (verbose) {
			print_bytes((unsigned char *)&req,sizeof(unsigned int));
		}
		request((unsigned char *)&req, sizeof(unsigned int), bufrecv);

		chunk_len = bufrecv[0];
		if (chunk_len==0) {
			break;
		} else if (chunk_len>127) {
			// TODO: server error
		}
		parse_response(bufrecv,chunk_len,treasure,&op,&op_param,&nonce);
		if (verbose) {
			printf("chunk length = %d\ttreasure = %s\top = 0x%x\top_param = 0x%x\tnonce = 0x%x\n",
					chunk_len,treasure,op,op_param,nonce);
		}
		treasure += chunk_len;
	}
	*treasure = '\0';
	printf("%s\n",full_treasure);

	close(sfd);
}


void prep_msg(unsigned char* buf, int level, int seed) {
	bzero(buf,BUFSIZE);

	buf[1] = level;

	unsigned int userid = (unsigned int)htonl(USERID);
	memcpy(&buf[2],&userid,sizeof(unsigned int));

	unsigned short seed_ns = htons(seed);
	memcpy(&buf[6],&seed_ns,sizeof(unsigned short));
}

void request(unsigned char *buf, size_t send_size, unsigned char *bufrecv) {
	ssize_t nwritten = sendto(sfd, buf, send_size, 0, remote_addr, addr_len);
	if (nwritten<0) {
		perror("sendto");
		exit(EXIT_FAILURE);
	}
	ssize_t nread = recvfrom(sfd, bufrecv, MAX_RECVSIZE, 0, remote_addr, &addr_len);
	if (nread<0) {
		perror("recvfrom");
		exit(EXIT_FAILURE);
	}
	if (verbose) {
		printf("Response is %d bytes:",(int)nread);
		print_bytes(bufrecv,nread);
	}
}

void parse_response(unsigned char *bufrecv, unsigned char chunk_len, unsigned char *treasure, unsigned char *op, unsigned short *op_param, unsigned int *nonce) {
	memcpy(treasure,&bufrecv[1],chunk_len);
	//treasure[chunk_len] = '\0';

	*op = bufrecv[chunk_len+1];

	unsigned short param_ns;
	memcpy(&param_ns,&bufrecv[chunk_len+2],sizeof(unsigned short));
	*op_param = ntohs(param_ns);

	unsigned int nonce_ns;
	memcpy(&nonce_ns,&bufrecv[chunk_len+4],sizeof(unsigned int));
	*nonce = (unsigned int)ntohl(nonce_ns);
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
