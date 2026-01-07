#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#include "sockhelper.h"

#define MAXEVENTS 64
#define MAXLINE 2048


struct client_info {
	int fd;
	int total_bytes_read;
	char desc[1024];
};

int main(int argc, char **argv) 
{
	/* Check usage */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int addr_fam = AF_INET;
	int sock_type = SOCK_STREAM;

	unsigned short port = atoi(argv[1]);

	int sfd;
	if ((sfd = socket(addr_fam, sock_type, 0)) < 0) {
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	// Declare structures for local address and port.
	//
	// Address information is stored in local_addr_ss, which is of type
	// struct addr_storage.  However, all functions require a parameter of
	// type struct sockaddr *.  Instead of type-casting everywhere, we
	// declare local_addr, which is of type struct sockaddr *, point it to
	// the address of local_addr_ss, and use local_addr everywhere.
	struct sockaddr_storage local_addr_ss;
	struct sockaddr *local_addr = (struct sockaddr *)&local_addr_ss;

	// Populate local_addr with the port using populate_sockaddr().
	populate_sockaddr(local_addr, addr_fam, NULL, port);
	if (bind(sfd, local_addr, sizeof(struct sockaddr_storage)) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sfd, 100) < 0) {
		perror("Could not listen");
		exit(EXIT_FAILURE);
	}

	// set listening file descriptor nonblocking
	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	// set the bit corresponding to sfd in rfds, to
	// register it for incoming connections, i.e., "read"
	// availability
	FD_SET(sfd, &rfds);

	struct client_info *fd_to_client_info[FD_SETSIZE];

	for (int i = 0; i < FD_SETSIZE; i++) {
		fd_to_client_info[i] = NULL;
	}

	while (1) {
		// Copy rfds to rfds_ready.  This is because rfds_ready will be
		// *read* by select() but also *changed* by select.  By using a
		// copy of rfds (rfds_ready), the value of rfds is not lost
		// after select().
		fd_set rfds_ready = rfds;

		// Wait for at least one fd to become ready.  Using NULL for
		// the last (timeout) argument means that it will wait
		// indefinitely.  While we are waiting for "read" events in
		// this program (rfds_ready), in a different scenario, we might
		// also be waiting for "write" events, in which case the third
		// argument (wfds_ready) would be non-NULL.
		int nready = select(FD_SETSIZE, &rfds_ready, NULL, NULL, NULL);

		for (int i = 0; i < FD_SETSIZE; i++) {
			if (i == sfd && FD_ISSET(i, &rfds_ready)) {

				printf("New read event for fd %d (listening file descriptor)\n", sfd);

				// loop until all pending clients have been accepted
				while (1) {
					// Declare structures for remote address and port.
					// See notes above for local_addr_ss and local_addr_ss.
					struct sockaddr_storage remote_addr_ss;
					struct sockaddr *remote_addr = (struct sockaddr *)&remote_addr_ss;
					char remote_ip[INET6_ADDRSTRLEN];
					unsigned short remote_port;

					socklen_t addr_len = sizeof(struct sockaddr_storage);
					int connfd = accept(sfd, remote_addr, &addr_len);
					if (connfd < 0) {
						if (errno == EWOULDBLOCK ||
								errno == EAGAIN) {
							// no more clients ready to accept
							break;
						} else {
							perror("accept");
							exit(EXIT_FAILURE);
						}
					}

					parse_sockaddr(remote_addr, remote_ip, &remote_port);
					printf("Connection from %s:%d\n",
							remote_ip, remote_port);

					/* UNCOMMENT FOR NONBLOCKING PART 2
					// set client file descriptor nonblocking
					if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
						fprintf(stderr, "error setting socket option\n");
						exit(1);
					}
					*/

					// allocate memory for a new struct
					// client_info, and populate it with
					// info for the new client
					struct client_info *new_client =
						(struct client_info *)malloc(sizeof(struct client_info));
					new_client->fd = connfd;
					new_client->total_bytes_read = 0;
					sprintf(new_client->desc, "Client %s:%d (fd %d)",
							remote_ip, remote_port, connfd);

					// Map the new file descriptor to the
					// newly allocated struct client_info.
					fd_to_client_info[connfd] = new_client;
					// Register the client file descriptor
					// for "read" events.
					FD_SET(connfd, &rfds);
				}
				// decrement nready; if the value is now 0, then we've
				// handled all the ready file descriptors.  This is
				// not required, but it helps us with efficiency.
				if ((--nready) == 0) {
					break;
				}
			} else if (FD_ISSET(i, &rfds_ready)) {
				// grab the struct client_info corresponding to the
				// file descriptor.
				struct client_info *active_client = fd_to_client_info[i];
				printf("New read event for fd %d (%s)\n",
						active_client->fd, active_client->desc);

				// read from socket and echo back the response.
				/* UNCOMMENT FOR NONBLOCKING PART 1
				while (1) {
				*/
					char buf[MAXLINE];
					int len = recv(active_client->fd, buf, MAXLINE, 0);
					if (len == 0) { // EOF received
						close(active_client->fd);
						// Remove the fd from rfds
						FD_CLR(active_client->fd, &rfds);
						free(active_client);
						fd_to_client_info[i] = NULL;
						/* UNCOMMENT FOR NONBLOCKING PART 1
						break;
						*/
					} else if (len < 0) {
						/* UNCOMMENT FOR NONBLOCKING PART 2
						if (errno == EWOULDBLOCK ||
								errno == EAGAIN) {
							// no more data to be read
						} else {
						*/
							perror("client recv");
							close(active_client->fd);
							// Remove the fd from rfds
							FD_CLR(active_client->fd, &rfds);
							free(active_client);
							fd_to_client_info[i] = NULL;
						/* UNCOMMENT FOR NONBLOCKING PART 2
						}
						*/
						/* UNCOMMENT FOR NONBLOCKING PART 1
						break;
						*/
					} else {
						active_client->total_bytes_read += len;
						printf("Received %d bytes (total: %d)\n", len,
								active_client->total_bytes_read);
						send(active_client->fd, buf, len, 0);
					}
				/* UNCOMMENT FOR NONBLOCKING PART 1
				}
				*/
				if ((--nready) == 0) {
					break;
				}
			}
		}
	}
}
