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


#endif // _UDP_H