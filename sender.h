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

int socket_send_next_frame(int fd);
void socket_send_frame(int fd, sequence_num_t sequence_number);
void socket_receive(int fd);
void socket_receive_ack(struct buffered_frame *bframe);

time_t get_timeout(void);
void service_timeout(int fd);

int epoll_setup(void);
int epoll_add(int epollfd, int fd);

void validate_cli_args(int argc, char *argv[]);


#endif // __SENDER_H