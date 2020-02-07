/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 2         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "udp.h"



int udp_client_init(char *host, char *port) {
	struct addrinfo hints;
	struct addrinfo *servinfo, *p;
	int sockfd, rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;// AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if (host == NULL) hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to obtain address info: %s\n",
			__FILE__,
			__LINE__,
			gai_strerror(rv)
		);
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		// If this is the listener, bind()
		if (host == NULL && bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}

		// If this is the sender, connect()
		if (host != NULL && connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to initialize a socket!\n",
			__FILE__,
			__LINE__
		);
		return -2;
	}

	freeaddrinfo(servinfo);
	return sockfd;
}


int udp_server_init(char *port) {
	return udp_client_init(NULL, port);
}


int udp_receive(int socket, void *buffer, size_t buffer_len, struct sockaddr *p, socklen_t *addr_len) {
	int recvlen;

	recvlen = recvfrom(socket, buffer, buffer_len, 0, p, addr_len);
	return recvlen;
}


int udp_send(int socket, void *buffer, size_t buffer_len, struct sockaddr *p, socklen_t addr_len) {
	int recvlen;

	recvlen = sendto(socket, buffer, buffer_len, 0, p, addr_len);
	return recvlen;
}