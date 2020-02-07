/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 2         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/

#ifndef _UDP_H
#define _UDP_H

#include <sys/socket.h>
#include <netdb.h>


int udp_client_init(char *host, char *port);
int udp_server_init(char *port);
int udp_receive(int socket, void *buffer, size_t buffer_len, struct sockaddr *p, socklen_t *addr_len);
int udp_send(int socket, void *buffer, size_t buffer_len, struct sockaddr *p, socklen_t addr_len);


#endif // _UDP_H