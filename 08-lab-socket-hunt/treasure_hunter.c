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

int verbose = 0;
int sfd = -1;
int addr_fam = AF_INET;
struct sockaddr *remote_addr = NULL, *local_addr = NULL;
char remote_ip[INET6_ADDRSTRLEN], local_ip[INET6_ADDRSTRLEN];
unsigned short remote_port, local_port;
socklen_t addr_len = sizeof(struct sockaddr_storage);

void prep_initial_msg(unsigned char* buf, int level, int seed);
void request(unsigned char *buf, size_t send_size, unsigned char *bufrecv);
void parse_response(unsigned char *bufrecv, unsigned char chunk_len, unsigned char *treasure, unsigned char *op, unsigned short *op_param, unsigned int *nonce);
unsigned int op_action(unsigned char op, unsigned short op_param, unsigned int nonce);
void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	// parse arguments
	if (argc != 5) {
		printf("Usage: ./treasure_hunter [server_domain_name] [port#] [level#] [seed#]\n");
		exit(EXIT_FAILURE);
	}
	char *server_domain_name = argv[1];
	char *port_in = argv[2];
	int level = atoi(argv[3]);
	if (level<0 || level>4) {
		printf("argument #3 must be a level number between 0 and 4\n");
		exit(EXIT_FAILURE);
	}
	int seed = atoi(argv[4]);

	// load arguments into message request
	unsigned char buf[BUFSIZE];
	prep_initial_msg(buf,level,seed);

	// query remote address info for UDP connection
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = addr_fam;
	hints.ai_socktype = SOCK_DGRAM;

	struct addrinfo *result;
	int s = getaddrinfo(server_domain_name,port_in,&hints,&result);
	if (s != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	// parse query response for remote address info
	struct sockaddr_storage remote_addr_ss;
	remote_addr = (struct sockaddr *)&remote_addr_ss;
	memcpy(remote_addr, result->ai_addr, sizeof(struct sockaddr_storage));
	parse_sockaddr(remote_addr, remote_ip, &remote_port);
	// initialize socket
	sfd = socket(addr_fam, SOCK_DGRAM, 0);
	if (sfd<0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	// capture local address info
	struct sockaddr_storage local_addr_ss;
	local_addr = (struct sockaddr *)&local_addr_ss;
	s = getsockname(sfd, local_addr, &addr_len);
	parse_sockaddr(local_addr, local_ip, &local_port);
	if (verbose) {
		printf("Connecting to %s:%d\n", remote_ip, remote_port);
		printf("Local port is %s:%d\n", local_ip, local_port);
	}

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
	if (chunk_len>127) {
		fprintf(stderr,"Server error 0x%x received\n", chunk_len);
		exit(EXIT_FAILURE);
	}
	parse_response(bufrecv,chunk_len,treasure,&op,&op_param,&nonce);
	if (verbose) {
		printf("chunk length = %d\ttreasure = %s\top = 0x%x\top_param = 0x%x\tnonce = 0x%x\n",
				chunk_len,treasure,op,op_param,nonce);
	}
	treasure += chunk_len;

	// enter request + response loop
	while (1) {
		// adjust follow-up request based on opcode
		req = op_action(op,op_param,nonce);
		if (verbose) {
			printf("Connecting to %s:%d\n", remote_ip, remote_port);
			printf("Local port is %s:%d\n", local_ip, local_port);
			printf("Request is:");
			print_bytes((unsigned char *)&req,sizeof(unsigned int));
		}

		// send follow-up request
		request((unsigned char *)&req, sizeof(unsigned int), bufrecv);

		// parse new response
		chunk_len = bufrecv[0];
		if (chunk_len==0) { // we got all the treasure
			break;
		} else if (chunk_len>127) {
			fprintf(stderr,"Server error 0x%x received\n", chunk_len);
			exit(EXIT_FAILURE);
		}
		parse_response(bufrecv,chunk_len,treasure,&op,&op_param,&nonce);
		if (verbose) {
			printf("chunk length = %d\ttreasure = %s\top = 0x%x\top_param = 0x%x\tnonce = 0x%x\n",
					chunk_len,treasure,op,op_param,nonce);
		}
		// advance treasure pointer for appending
		treasure += chunk_len;
	}
	// null-terminate completed treasure and print
	*treasure = '\0';
	printf("%s\n",full_treasure);

	close(sfd);
}


// initial request holds several data items
void prep_initial_msg(unsigned char* buf, int level, int seed) {
	bzero(buf,BUFSIZE);

	buf[1] = level;

	unsigned int userid = (unsigned int)htonl(USERID);
	memcpy(&buf[2],&userid,sizeof(unsigned int));

	unsigned short seed_ns = htons(seed);
	memcpy(&buf[6],&seed_ns,sizeof(unsigned short));
}

// send message stored in *buf to the server and collect the response into *bufrecv
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

// extract integer values of various sizes from *bufrecv
void parse_response(unsigned char *bufrecv, unsigned char chunk_len, unsigned char *treasure, unsigned char *op, unsigned short *op_param, unsigned int *nonce) {
	memcpy(treasure,&bufrecv[1],chunk_len);

	*op = bufrecv[chunk_len+1];

	unsigned short param_ns;
	memcpy(&param_ns,&bufrecv[chunk_len+2],sizeof(unsigned short));
	*op_param = ntohs(param_ns);

	unsigned int nonce_ns;
	memcpy(&nonce_ns,&bufrecv[chunk_len+4],sizeof(unsigned int));
	*nonce = (unsigned int)ntohl(nonce_ns);
}

// adjust local/remote ports or collect extra context based on opcode
unsigned int op_action(unsigned char op, unsigned short op_param, unsigned int nonce) {
	switch((int)op) {
	case 0:
		return (unsigned int)htonl(nonce+1);
	case 1:
		// update remote port to match most recently received opcode parameter
		remote_port = op_param;
		populate_sockaddr(remote_addr,addr_fam,remote_ip,remote_port);
		return (unsigned int)htonl(nonce+1);
	case 2:
		// update local port to match opcode parameter
		local_port = op_param;
		populate_sockaddr(local_addr,addr_fam,local_ip,local_port);
		// close socket and reopen with new local port config
		close(sfd);
		sfd = socket(addr_fam, SOCK_DGRAM, 0);
		if (sfd<0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}
		if (bind(sfd, local_addr, sizeof(struct sockaddr_storage)) < 0) {
			perror("bind()");
			exit(EXIT_FAILURE);
		}
		return (unsigned int)htonl(nonce+1);
	case 3:
	default:
		printf("Op-code %d not yet implemented. Exiting\n", (int)op);
		exit(EXIT_SUCCESS);
	}
}

// print contents of buffer with side-by-side hex/character display
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
