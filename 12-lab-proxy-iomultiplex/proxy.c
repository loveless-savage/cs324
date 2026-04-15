#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include "sockhelper.h"
#include "sbuf.h"

#define MAX_OBJECT_SIZE 102400
#define REQ_SIZE 1024
#define RES_SIZE 16384
#define FD_COUNT 1024
/* Request states */
#define READ_REQUEST 1
#define SEND_REQUEST 2
#define READ_RESPONSE 3
#define SEND_RESPONSE 4

static const char *user_agent_hdr = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
static const int verbose = 0;

int  complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
int  open_sfd(unsigned short);
void handle_new_clients(int);
void handle_client(int);
void prepare_request(int);
void test_parser();
void print_bytes(unsigned char *, int);

fd_set rfds, wfds;
struct request_info {
	int cfd, rfd;
	int request_state;
	char *req_buf, *req_head, *res_buf, *res_head;
	int res_bytes_recv;
};
struct request_info *reqdb[FD_COUNT];


int main(int argc, char *argv[]) {
	// cmd argument 1 = port#
	if (argc<2) {
		printf("Usage: %s [port]\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	unsigned short port_listen = (unsigned short)atoi(argv[1]);
	
	// initialize fd_sets
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	// open port for receiving requests
	if (verbose) printf("opening listening port: ");
	int sfd = open_sfd(port_listen);
	if (verbose) printf("done\n");
	FD_SET(sfd, &rfds);

	// accept connections as they come in
	while(1) {
		if (verbose) printf("running select()\n");
		// wait for incoming connection(s)
		fd_set rfds_ready=rfds, wfds_ready=wfds;
		int nready = select(FD_SETSIZE, &rfds_ready, &wfds_ready, NULL, NULL);
		if (nready<0) {
			perror("select()");
			exit(EXIT_FAILURE);
		}
		// check each file descriptor for ready state
		for (int i = 0; i < FD_SETSIZE; i++) {
			// client-listening port needs to accept new connections
			if (i == sfd && FD_ISSET(i, &rfds_ready)) {
				handle_new_clients(sfd);
				if ((--nready) == 0) break;
			} else if (FD_ISSET(i, &rfds_ready) || FD_ISSET(i, &wfds_ready)) {
				handle_client(i);
				if ((--nready) == 0) break;
			}
		}
	}
	return 0;
}

int complete_request_received(char *request) {
	return strstr(request,"\r\n\r\n")!=NULL;
}

void parse_request(char *request, char *method,
		char *hostname, char *port, char *path) {
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
	// set listening file descriptor nonblocking
	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}
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

void handle_new_clients(int sfd) {
	struct sockaddr_storage request_addr_ss;
	struct sockaddr *request_addr = (struct sockaddr *)&request_addr_ss;
	socklen_t addr_len = sizeof(struct sockaddr_storage);
	int cfd;
	while(1) {
		cfd = accept(sfd,request_addr,&addr_len);
		if (cfd<0) {
			if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
				perror("accept()");
				exit(EXIT_FAILURE);
			}
			return;
		}
		printf("accepted connection: cfd = %d\n",cfd);
		
		char request_ip[INET6_ADDRSTRLEN];
		unsigned short request_port;
		parse_sockaddr(request_addr, request_ip, &request_port);
		printf("Connection from %s:%d\n",
				request_ip, request_port);
		// set client file descriptor nonblocking
		if (fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
			fprintf(stderr, "error setting socket option\n");
			exit(1);
		}
		// allocate new request_info for current connection
		struct request_info *new_client =
			(struct request_info *)malloc(sizeof(struct request_info));
		new_client->cfd = cfd;
		new_client->request_state = READ_REQUEST;
		new_client->req_buf = (char *)malloc(REQ_SIZE*sizeof(char));
		new_client->req_head = new_client->req_buf;
		// add to global request database
		reqdb[cfd] = new_client;
		// register our new client file descriptor on the select() watchlist
		FD_SET(cfd, &rfds);
	}
}

void handle_client(int i) {
	if (verbose) printf("handling client fd(%d)=STATE %d\n",i,reqdb[i]->request_state);
	char *buf, *head;
	switch (reqdb[i]->request_state) {
		case READ_REQUEST:
			// collect request from client connection
			buf = reqdb[i]->req_buf;
			head = reqdb[i]->req_head;
			while (1) {
				ssize_t nread = recv(reqdb[i]->cfd, head, REQ_SIZE-(head-buf), 0);
				if (nread < 0) {
					// if we got an error
					if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
						perror("recv()");
						FD_CLR(reqdb[i]->cfd, &rfds);
						close(reqdb[i]->cfd);
						free(reqdb[i]);
						return;
					}
					// otherwise we can resume when more bytes are available
					reqdb[i]->req_head = head;
					return;
				// if client closes, abort transaction
				} else if (nread == 0) {
					FD_CLR(reqdb[i]->cfd, &rfds);
					close(reqdb[i]->cfd);
					return;
				}
				head += nread;
				// when we receive the full header, stop reading from the socket
				if (complete_request_received(buf)) {
					*++head = '\0';
					reqdb[i]->req_head = head;
					break;
				}
			}
			prepare_request(i);
		case SEND_REQUEST:
			if (verbose) printf("sending request fd(%d)\n",i);
			// send request
			buf = reqdb[i]->req_buf;
			head = reqdb[i]->req_head;
			while (1) {
				ssize_t nwritten = send(reqdb[i]->rfd,
						head, strlen(buf) - (head-buf), 0);
				if (nwritten < 0) {
					// if we got an error
					if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
						perror("send()");
						FD_CLR(reqdb[i]->rfd, &wfds);
						close(reqdb[i]->rfd);
						close(reqdb[i]->cfd);
						if (verbose) printf("freeing reqdb[%d]\n",i);
						free(reqdb[i]);
						return;
					}
					// otherwise we can resume when more bytes are available
					if (verbose) printf("sending request fd(%d) blocking\n",i);
					reqdb[i]->req_head = head;
					return;
					// if client closes, abort transaction
				}
				head += nwritten;
				// when we finish writing to the server, switch states
				if (strlen(buf) == (head-buf)) {
					if (verbose) printf("request sent fd(%d)\n",i);
					// prepare buffers for server response
					reqdb[i]->res_buf = (char *)malloc(RES_SIZE*sizeof(char));
					reqdb[i]->res_head = reqdb[i]->res_buf;
					// adjust state of client socket
					FD_CLR(reqdb[i]->rfd, &wfds);
					FD_SET(reqdb[i]->rfd, &rfds);
					reqdb[i]->request_state = READ_RESPONSE;
					// give a pointer to this database entry at rfd in addition to cfd
					reqdb[reqdb[i]->rfd] = reqdb[i];
					return;
				}
			}
		case READ_RESPONSE:
			if (verbose) printf("receiving response fd(%d)\n",i);
			// collect response from server
			buf = reqdb[i]->res_buf;
			head = reqdb[i]->res_head;
			while (1) {
				ssize_t nread = recv(reqdb[i]->rfd, head, RES_SIZE-(head-buf), 0);
				if (nread < 0) {
					// if we got an error
					if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
						perror("recv()");
						FD_CLR(reqdb[i]->rfd, &rfds);
						close(reqdb[i]->rfd);
						if (verbose) printf("freeing reqdb[%d]\n",i);
						free(reqdb[i]);
						return;
					}
					// otherwise we can resume when more bytes are available
					if (verbose) printf("receiving response fd(%d) blocking\n",i);
					reqdb[i]->res_head = head;
					return;
				}
				head += nread;
				// if server closes the connection, we are ready to pass it along
				if (nread == 0) {
					reqdb[i]->res_head = head;
					close(reqdb[i]->rfd);
					reqdb[i]->res_bytes_recv = head-buf;
					reqdb[i]->res_head = buf;
					if (verbose) {
						printf("received %li bytes\n",head-buf);
						print_bytes((unsigned char *)buf,head-buf);
					}
					// adjust state of client and server sockets
					FD_CLR(reqdb[i]->rfd, &rfds);
					FD_SET(reqdb[i]->cfd, &wfds);
					reqdb[i]->request_state = SEND_RESPONSE;
					return;
				}
			}
		case SEND_RESPONSE:
			if (verbose) printf("sending response fd(%d)\n",i);
			// send request
			buf = reqdb[i]->res_buf;
			head = reqdb[i]->res_head;
			while (1) {
				ssize_t nwritten = send(reqdb[i]->cfd,
						head, reqdb[i]->res_bytes_recv - (head-buf), 0);
				if (nwritten < 0) {
					// if we got an error
					if (errno!=EAGAIN && errno!=EWOULDBLOCK) {
						perror("send()");
						FD_CLR(reqdb[i]->cfd, &wfds);
						close(reqdb[i]->cfd);
						if (verbose) printf("freeing reqdb[%d]\n",i);
						free(reqdb[i]);
						return;
					}
					// otherwise we can resume when more bytes are available
					if (verbose) printf("sending response fd(%d) blocking\n",i);
					reqdb[i]->res_head = head;
					return;
					// if client closes, abort transaction
				}
				//printf("nwritten = %d\n",nwritten);print_bytes(head,nwritten);fflush(stdout);
				head += nwritten;
				// when we finish writing to the server, switch states
				if (reqdb[i]->res_bytes_recv == (head-buf)) {
					if (verbose) printf("response sent %li bytes fd(%d)\n",head-buf,i);
					// adjust state of client socket
					FD_CLR(reqdb[i]->cfd, &wfds);
					close(reqdb[i]->cfd);
					printf("transaction complete on cfd = %d\n",reqdb[i]->cfd);
					// clear this transaction from database
					if (verbose) printf("freeing reqdb[%d]\n",i);
					free(reqdb[i]);
					return;
				}
			}
	}
}

void prepare_request(int i) {
	if (verbose) printf("preparing request fd(%d)\n",i);
	// parse client request
	char method[16], hostname[64], port[8], path[64];
	parse_request(reqdb[i]->req_buf, method, hostname, port, path);
	if (verbose) {
		printf("total bytes received: %d",(int)(reqdb[i]->req_head-reqdb[i]->req_buf));
		print_bytes((unsigned char *)reqdb[i]->req_buf,reqdb[i]->req_head-reqdb[i]->req_buf);
		printf("METHOD: %s\n", method);
		printf("HOSTNAME: %s\n", hostname);
		printf("PORT: %s\n", port);
		printf("PATH: %s\n", path);
	}
	// build request for server
	sprintf(reqdb[i]->req_buf,
			"%s %s HTTP/1.0\r\nHost: %s:%s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
			method, path, hostname, port, user_agent_hdr);
	if (verbose) {
		printf("@@@ REQUEST = \n%s\n",reqdb[i]->req_buf);
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
		FD_CLR(reqdb[i]->cfd, &rfds);
		close(reqdb[i]->cfd);
		return;
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
		FD_CLR(reqdb[i]->cfd, &rfds);
		close(reqdb[i]->cfd);
		return;
	}
	// set client file descriptor nonblocking
	if (fcntl(rfd, F_SETFL, fcntl(rfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}
	// add server file descriptor to client database
	reqdb[i]->rfd = rfd;
	reqdb[i]->req_head = reqdb[i]->req_buf;
	// adjust state of client and server sockets
	FD_CLR(reqdb[i]->cfd, &rfds);
	FD_SET(reqdb[i]->rfd, &wfds);
	reqdb[i]->request_state = SEND_REQUEST;
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

