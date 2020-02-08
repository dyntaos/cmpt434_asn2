/**********************************
 **           CMPT 434           **
 **  University of Saskatchewan  **
 **         Assignment 2         **
 **----------------------------- **
 **          Kale Yuzik          **
 **     kay851@mail.usask.ca     **
 **      kay851    11071571      **
 **********************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/epoll.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sender.h"
#include "udp.h"


#define EPOLL_EVENT_COUNT		2


struct buffered_frame **sequenced_frames;
sequence_num_t sending_window_start = INITIAL_SEQ_NUM;
sequence_num_t sending_window_end = INITIAL_SEQ_NUM;
sequence_num_t max_sequence_num = MAX_SEQ_NUM;
size_t sending_window_size = 1;
size_t unackd_frames = 0;
uint16_t send_timeout = 1;
char *receiver_host, *receiver_port;




void send_enqueue(struct buffered_frame *bframe) {
	struct send_buffer_item *sbi;
	sbi = (struct send_buffer_item*) malloc(sizeof(struct send_buffer_item));
	sbi->bframe = bframe;
	TAILQ_INSERT_HEAD(&send_buffer_head, sbi, next_item);
	send_buffer_size++;
}


struct buffered_frame *send_dequeue(void) {
	struct send_buffer_item *sbi;
	struct buffered_frame *frame;

	sbi = TAILQ_LAST(&send_buffer_head, send_buffer_head_t);
	TAILQ_REMOVE(&send_buffer_head, sbi, next_item);
	send_buffer_size--;
	frame = sbi->bframe;
	free(sbi);
	return frame;
}



struct buffered_frame *create_buffered_frame(void *data, size_t data_len) {
	static uint8_t next_sequence_number = INITIAL_SEQ_NUM;
	struct buffered_frame *bframe;

	bframe = (struct buffered_frame*) malloc(sizeof(struct buffered_frame));
	if (bframe == NULL) {
		return NULL;
	}
	bframe->frame.sequence_number = next_sequence_number;
	bframe->frame.frame_type =
		next_sequence_number == max_sequence_num ?
			FRAME_TYPE_DATA_WITH_SEQ_RESET : FRAME_TYPE_DATA;
	next_sequence_number = (next_sequence_number + 1) % (max_sequence_num + 1);
	bframe->frame.payload_length = data_len;
	bframe->state = UNSENT;
	bframe->data = data;

	return bframe;
}



int socket_send_next_frame(int fd) {

	if (unackd_frames >= sending_window_size - 1) {
		return -1;
	}

	if (send_buffer_size <= 0) {
		return -2;
	}

	printf("<DEBUG> Sending next frame...\n"); // TODO


	// If the frame in the next slot in the circular buffer was
	// not previously empty, free the data pointer in it.
	if (sequenced_frames[sending_window_end] != NULL && sequenced_frames[sending_window_end]->data != NULL) {
		free(sequenced_frames[sending_window_end]->data);
		sequenced_frames[sending_window_end]->data = NULL;
	}

	sequenced_frames[sending_window_end] = send_dequeue();

	unackd_frames++;

	socket_send_frame(fd, sending_window_end);

	sending_window_end = (sending_window_end + 1) % (max_sequence_num + 1);

	return 1; // TODO
}



int socket_send_frame(int fd, sequence_num_t sequence_number) {
	int sent_len;

	if (sequence_number > max_sequence_num) {
		fprintf(
			stderr,
			"[%s : %d]: Sequence number provided (%u) exceeds the maximum sequence number (%u)!\n",
			__FILE__,
			__LINE__,
			sequence_number,
			max_sequence_num
		);
		return -1;
	}

	if (sequenced_frames[sequence_number]->state == ACKD) {
		fprintf(
			stderr,
			"[%s : %d]: Sequence number provided (%u) has already been ACK'd!\n",
			__FILE__,
			__LINE__,
			sequence_number
		);
		return -2;
	}

	sequenced_frames[sequence_number]->state = SENT;

	printf("<DEBUG> Send()\n"); // TODO

	sent_len = sendto(
		fd,
		(void*) &sequenced_frames[sequence_number]->frame,
		sizeof(struct frame),
		0,
		NULL,
		0
	);

	if (sent_len != sizeof(struct frame)) {
		perror("");
		fprintf(
			stderr,
			"[%s : %d]: Failed to send header of frame with sequence number %u!\n",
			__FILE__,
			__LINE__,
			sequence_number
		);
		exit(EXIT_FAILURE);
	}

	sent_len = sendto(
		fd,
		(void*) sequenced_frames[sequence_number]->data,
		sequenced_frames[sequence_number]->frame.payload_length,
		0,
		NULL,
		0
	);

	if (sent_len != sequenced_frames[sequence_number]->frame.payload_length) {
		perror("");
		fprintf(
			stderr,
			"[%s : %d]: Failed to send payload of frame with sequence number %u; send() returned %d...\n",
			__FILE__,
			__LINE__,
			sequence_number,
			sent_len
		);
		exit(EXIT_FAILURE);
	}

	return 1; // TODO
}



int socket_send_timeout(int fd) {
	// TODO: Retransmit unack'd frames in sending window
	(void) fd; // TODO
	return 1; // TODO
}



int socket_receive(int fd) {
	struct buffered_frame *bframe;
	int recv_len;

	bframe = (struct buffered_frame*) malloc(sizeof(struct buffered_frame));
	bframe->data = NULL;
	bframe->sent_time = 0;
	bframe->state = RECVD;

	recv_len = recvfrom(fd, (void*) &bframe->frame, sizeof(struct frame), 0, NULL, 0);

	if (recv_len <= 0) {
		fprintf(stderr, "[%s : %d]: Connection with receiver closed...\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE); // TODO: Failure?

	} else if (recv_len != sizeof(struct frame)) {
		fprintf(stderr, "[%s : %d]: Received unexpected number of bytes...\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if (bframe->frame.frame_type == FRAME_TYPE_ACK) {
		if (bframe->frame.payload_length != 0) {
			// Ensure an ACK isn't sent with a payload, thus throwing
			// the sender and receiver stream (DGRAM) out of sync
			fprintf(stderr, "[%s : %d]: Received ACK with non-zero byte payload!\n", __FILE__, __LINE__);
		}

		socket_receive_ack(bframe);

	} else if (bframe->frame.frame_type == FRAME_TYPE_DATA || bframe->frame.frame_type == FRAME_TYPE_DATA_WITH_SEQ_RESET) {
		fprintf(stderr, "[%s : %d]: Sender received data frame!\n", __FILE__, __LINE__);

		// Read the number of payload bytes described in the header, so the connection remains synchronized
		bframe->data = (char*) malloc(bframe->frame.payload_length);
		if (bframe->data == NULL) {
			fprintf(stderr, "[%s : %d]: Failed to allocate data buffer!\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}

		recv_len = recvfrom(fd, (void*) bframe->data, bframe->frame.payload_length, 0, NULL, 0);

		if (recv_len < 0 || recv_len != bframe->frame.payload_length) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to receive the expected number of bytes from socket!\n",
				__FILE__,
				__LINE__
			);
			exit(EXIT_FAILURE);
		}

	} else {
		fprintf(
			stderr,
			"[%s : %d]: Sender received unknown frame type (%u)!\n",
			__FILE__,
			__LINE__,
			bframe->frame.frame_type
		);
	}

	free(bframe->data);
	free(bframe);
	return 1; // TODO
}



void socket_receive_ack(struct buffered_frame *bframe) {

	switch (sequenced_frames[bframe->frame.sequence_number]->state) {
		case UNSENT:
			fprintf(stderr, "[%s : %d]: Sender received ACK for unsent frame!\n", __FILE__, __LINE__);
			break;

		case SENT:
			sequenced_frames[bframe->frame.sequence_number]->state = ACKD;
			unackd_frames--;
			while (sequenced_frames[sending_window_start] != NULL && sequenced_frames[sending_window_start]->state == ACKD) {
				sending_window_start = (sending_window_start + 1) % (max_sequence_num + 1);
			}
			printf("Received ACK for sequence %u\n", bframe->frame.sequence_number);
			break;

		case RECVD:
			// TODO?
			// The sender should not ever see a frame with the state RECV
			printf("[%s : %d]: socket_receive_ack() RECVD\n", __FILE__, __LINE__);
			break;

		case ACKD:
			// Reawknowledement of frame -- disregard
			// TODO?
			printf("[%s : %d]: socket_receive_ack() ACKD\n", __FILE__, __LINE__);
			break;
	}
}



int epoll_setup(void) {
	int epollfd;

	epollfd = epoll_create1(0);

	if (epollfd < 0) {
		// TODO
		perror("epoll_create1");
		return -1;
	}
	return epollfd;
}



int epoll_add(int epollfd, int fd) {
	struct epoll_event event;

	event.data.fd = fd;
	event.events = EPOLLIN; // TODO: EPOLLET??

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event)) {
		// TODO
		perror("epoll_ctl");
		return -1;
	}
	return 0;
}



void validate_cli_args(int argc, char *argv[]) {
	receiver_host = argv[1];
	receiver_port = argv[2];

	if (argc != 5) {
		printf("Usage: %s ReceiverHostname ReceiverPort MaxSendingWindowSize TimeoutSeconds\n\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (strlen(receiver_host) < 2) {
		fprintf(stderr, "Provide a valid server hostname or IP address\n");
		exit(EXIT_FAILURE);
	}

	if (strlen(receiver_port) > 5) {
		fprintf(stderr, "Invalid receiver port number\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < strlen(receiver_port); i++) {
		if (!isdigit(receiver_port[i])) {
			fprintf(stderr, "The receiver port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	if (strtoul(receiver_port, NULL, 10) > 65535) {
		fprintf(stderr, "Receiver port number must be between 0 to 65535\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < strlen(argv[3]); i++) {
		if (!isdigit(argv[3][i])) {
			fprintf(stderr, "The maximum sending window size provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	// TODO: Sending window > 0
	// TODO: Timeout > 0

	for (size_t i = 0; i < strlen(argv[4]); i++) {
		if (!isdigit(argv[4][i])) {
			fprintf(stderr, "The timeout value provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	send_timeout = strtol(argv[4], NULL, 10);

	sending_window_size = strtol(argv[3], NULL, 10);
	if (sending_window_size + 1 >= max_sequence_num) { // TODO: Confirm inequaility
		max_sequence_num = sending_window_size + 1; // TODO: Confirm
	}
}


int main(int argc, char *argv[]) {
	int sockfd, epollfd, epoll_count;
	char *input = NULL, *queue_text;
	size_t input_size = 0;
	struct buffered_frame *bframe;
	struct epoll_event events[EPOLL_EVENT_COUNT];

	TAILQ_INIT(&send_buffer_head);

	validate_cli_args(argc, argv);

	sequenced_frames = (struct buffered_frame**) malloc((sizeof(struct buffered_frame) * max_sequence_num) + 1);

	if (sequenced_frames == NULL) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to allocate memory for circular sending window bufffer!\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}
	memset(sequenced_frames, 0, (sizeof(struct buffered_frame) * max_sequence_num) + 1);

	sockfd = udp_client_init(receiver_host, receiver_port);
	if (sockfd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to open socket...\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}

	epollfd = epoll_setup();
	if (epollfd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to create epoll file descriptor\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}

	epoll_add(epollfd, STDIN_FILENO);
	epoll_add(epollfd, sockfd);

	for (;;) {
		//printf("<DEBUG> Calling epoll_wait()...\n"); // TODO

		epoll_count = epoll_wait(epollfd, events, EPOLL_EVENT_COUNT, 2000); // TODO: Timeout
		if (epoll_count == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (epoll_count == 0) {
			//printf("<DEBUG> epoll_wait() timed out...\n"); // TODO
			continue;
		}

		for (int i = 0; i < epoll_count; i++) {
			if (events[i].data.fd == STDIN_FILENO) {
				// STDIN
				getline(&input, &input_size, stdin);
				//printf("<DEBUG> Read from stdin: %s\n", input); // TODO

				queue_text = (char*) malloc(input_size + 1);
				if (queue_text == NULL) {
					fprintf(
						stderr,
						"[%s : %d]: malloc() failed to allocate string buffer!\n",
						__FILE__,
						__LINE__
					);
				}

				strncpy(queue_text, input, input_size);
				bframe = create_buffered_frame(queue_text, input_size); // TODO: input_size or input_size + 1?

				if (bframe == NULL) {
					fprintf(
						stderr,
						"[%s : %d]: Failed to allocate memory for buffered frame!\n",
						__FILE__,
						__LINE__
					);
					exit(EXIT_FAILURE);
				}

				send_enqueue(bframe);

				// TODO******

				// Send until the sending window is full or there are no more messages to send
				while (socket_send_next_frame(sockfd) >= 0);

			} else if (events[i].data.fd == sockfd) {
				// SOCKET

				socket_receive(sockfd); // TODO: Will this eventually return an error to test for?

			} else {
				fprintf(
					stderr,
					"[%s : %d]: epoll_wait() returned an unknown file descriptor number!\n",
					__FILE__,
					__LINE__
				);
				exit(EXIT_FAILURE);
			}
		}
	}

	if (input != NULL) free(input);

	return EXIT_SUCCESS;
}
