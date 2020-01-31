/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 2         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/

#ifndef __SENDER_H
#define __SENDER_H


#include <sys/queue.h>

#include "frame.h"



/****************************************
 *             Send Buffer              *
 ****************************************/
TAILQ_HEAD(send_buffer_head_t, send_buffer_item) send_buffer_head = TAILQ_HEAD_INITIALIZER(send_buffer_head);

struct send_buffer_head_t *send_buffer_headp;

size_t send_buffer_size = 0;

struct send_buffer_item {
	struct buffered_frame *bframe;
	TAILQ_ENTRY(send_buffer_item) next_item;
};


void send_enqueue(struct buffered_frame *bframe);
struct buffered_frame *send_dequeue(void);

struct buffered_frame *create_buffered_frame(void *data, size_t data_len);

int socket_init(char *host, char *port);
int socket_send_next_frame(int fd);
int socket_send_timeout(int fd);
int socket_receive(int fd);
int socket_receive_ack(int fd, struct buffered_frame *bframe);

void validate_cli_args(int argc, char *argv[]);


#endif // __SENDER_H