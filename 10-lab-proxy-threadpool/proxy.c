#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "sockhelper.h"

/* Recommended max object size */
#define MAX_OBJECT_SIZE 102400
#define BUF_SIZE 1024

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
static const int verbose = 1;

int  complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
int  open_sfd(unsigned short);
void handle_client(int);
void test_parser();
void print_bytes(unsigned char *, int);


int main(int argc, char *argv[]) {
	if (argc<2) {
		printf("Usage: %s [port]\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	unsigned short port_listen = (unsigned short)atoi(argv[1]);
	printf("opening listening port: ");fflush(stdout);
	int sfd = open_sfd(port_listen);
	printf("done\n");fflush(stdout);
	struct sockaddr_storage remote_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
	socklen_t addr_len = sizeof(struct sockaddr_storage);
	while(1) {
		printf("waiting for connection: ");fflush(stdout);
		int cfd = accept(sfd,remote_addr,&addr_len);
		printf("accepted\n");fflush(stdout);
		handle_client(cfd);
	}
	//printf("%s\n", user_agent_hdr);
	return 0;
}

int complete_request_received(char *request) {
	return strstr(request,"\r\n\r\n")!=NULL;
}

void parse_request(char *request, char *method, char *hostname, char *port, char *path) {
	// tokenize first space-delimited word of request as method
	char *head = request, *tail = strstr(request," ");
	strncpy(method, head, tail-head);
	method[tail-head] = '\0';
	// find the URL and remove the leading "http://"
	head = tail+1;
	if(strstr(head,"://")!=NULL) head = (strstr(head,"://")+3);
	tail = strstr(head,"/");
	// separate hostname and port if possible
	if(strstr(head,":")!=NULL && strstr(head,":")<tail) {
		tail = strstr(head,":");
		strncpy(hostname, head, tail-head);
		hostname[tail-head] = '\0';
		head = tail+1;
		tail = strstr(head,"/");
		strncpy(port, head, tail-head);
		port[tail-head] = '\0';
	// if there is no port specified, default port is '80'
	} else {
		strncpy(hostname, head, tail-head);
		hostname[tail-head] = '\0';
		strncpy(port, "80\0", 3);
	}
	// the remainder of the URL is the path
	head = tail;
	tail = strstr(head," ");
	strncpy(path, head, tail-head);
	path[tail-head] = '\0';
}

int open_sfd(unsigned short port_listen) {
	// initialize socket
	int sfd;
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
		perror("socket");
		exit(EXIT_FAILURE);
	}
	int optval = 1;
	// allow socket to bind to an already-in-use address and port
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	// bind to hosting port and prepare to listen for connections
	struct sockaddr_storage local_addr_ss;
	struct sockaddr *local_addr = (struct sockaddr *)&local_addr_ss;
	populate_sockaddr(local_addr, AF_INET, NULL, port_listen);
	if (bind(sfd, local_addr, sizeof(struct sockaddr_storage)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sfd,100) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	return sfd;
}

void handle_client(int cfd) {
	char buf[BUF_SIZE];
	char *head = buf;
	while (1) {
		ssize_t nread = recv(cfd, head, BUF_SIZE-(head-buf), 0);
		if (nread < 0) {
			perror("receiving request");
			exit(EXIT_FAILURE);
			// if client closes, prepare for the next client connection
		} else if (nread == 0) {
			close(cfd); // FIXME
			return;
		}
		head += nread;
		// when we receive the full header, stop reading from the socket
		if (complete_request_received(buf)) {
			*++head = '\0';
			break;
		}
	}
	// parse client request
	char method[16], hostname[64], port[8], path[64];
	parse_request(buf, method, hostname, port, path);
	if (verbose) {
		printf("total bytes received: %d",(int)(head-buf));
		print_bytes((unsigned char *)buf,head-buf);
		printf("METHOD: %s\n", method);
		printf("HOSTNAME: %s\n", hostname);
		printf("PORT: %s\n", port);
		printf("PATH: %s\n", path);
	}
	// build request for server
	char request[BUF_SIZE];
	sprintf(request,
			"%s %s HTTP/1.0\r\nHost: %s:%s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
			method, path, hostname, port, user_agent_hdr);
	if (verbose) {
		printf("@@@ REQUEST = \n%s\n",request);
	}

	// query remote address info for TCP connection
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo *result;
	int s = getaddrinfo(hostname,port,&hints,&result);
	if (s != 0) {
		fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	// parse query response for remote address info
	struct sockaddr_storage remote_addr_ss;
	struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
	memcpy(remote_addr, result->ai_addr, sizeof(struct sockaddr_storage));
	char remote_ip[INET6_ADDRSTRLEN];
	unsigned short remote_port;
	parse_sockaddr(remote_addr, remote_ip, &remote_port);
	// open socket to remote server
	int rfd = socket(result->ai_family, result->ai_socktype, 0);
	if (rfd < 0 ||
			connect(rfd, remote_addr, sizeof(struct sockaddr_storage))<0) {
		fprintf(stderr,"socket: failed to connect to hostname %s\n",hostname);
		exit(EXIT_FAILURE);
	}
	// send request
	ssize_t nwritten = send(rfd, request, strlen(request), 0);
	if (nwritten < 0) {
		perror("send");
		exit(EXIT_FAILURE);
	}
	// collect response
	char response[MAX_OBJECT_SIZE];
	head = response;
	while (1) {
		ssize_t nread = recv(rfd, head, MAX_OBJECT_SIZE-(head-response), 0);
		if (nread < 0) {
			perror("receiving response");
			exit(EXIT_FAILURE);
			// if client closes, prepare for the next client connection
		} else if (nread == 0) {
			close(rfd);
			break;
		}
		head += nread;
	}
	if (verbose) {
		printf("total bytes received: %d",(int)(head-response));
		print_bytes((unsigned char *)response,head-response);
	}

	// Return bytes to remote socket using sendto().
	if (send(cfd, response, head-response, 0) < 0) {
		perror("sending response");
	}
	close(cfd);
}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64];

	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (complete_request_received(reqs[i])) {
			printf("REQUEST COMPLETE\n");
			parse_request(reqs[i], method, hostname, port, path);
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("PATH: %s\n", path);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
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
