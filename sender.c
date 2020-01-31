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

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sender.h"


struct buffered_frame **sequenced_frames;
sequence_num_t sending_window_start = INITIAL_SEQ_NUM;
sequence_num_t sending_window_end = INITIAL_SEQ_NUM;
sequence_num_t max_sequence_num = MAX_SEQ_NUM;
size_t sending_window_size = 1;
size_t unackd_frames = 0;
uint16_t send_timeout = 1;




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
	bframe->frame.sequence_number = next_sequence_number;
	next_sequence_number = (next_sequence_number + 1) % (max_sequence_num + 1);
	bframe->frame.payload_length = data_len;
	bframe->state = UNSENT;
	bframe->data = data;

	return bframe;
}



int socket_init(char *host, char *port) {
	struct addrinfo hints;
	struct addrinfo *ainfo, *p;
	int sockfd, rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if (host == NULL) hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(host, port, &hints, &ainfo)) != 0) {
		fprintf(
			stderr,
			"[%s:%d]: Failed to obtain address info: %s\n",
			__FILE__,
			__LINE__,
			gai_strerror(rv)
		);
		return -1;
	}

	for (p = ainfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "[%s:%d]: Failed to bind socket\n", __FILE__, __LINE__);
		return -2;
	}

	freeaddrinfo(ainfo);
	return sockfd;
}



int socket_send_next_frame(int fd) {

	if (unackd_frames >= sending_window_size) {
		fprintf(
			stderr,
			"[%s:%d]: Attempt to send additional frame, when sending window is at its maximum size!\n",
			__FILE__,
			__LINE__
		);
		return -1;
	}

	if (send_buffer_size <= 0) {
		fprintf(
			stderr,
			"[%s:%d]: Attempt to send additional frame, when sending buffer is empty!\n",
			__FILE__,
			__LINE__
		);
		return -1;
	}

	sending_window_end = (sending_window_end + 1) % (max_sequence_num + 1);

	// If the frame in the next slot in the circular buffer was
	// not previously empty, free the data pointer in it.
	if (sequenced_frames[sending_window_end]->data != NULL) {
		free(sequenced_frames[sending_window_end]->data);
		sequenced_frames[sending_window_end]->data = NULL;
	}

	sequenced_frames[sending_window_end] = send_dequeue();

	unackd_frames++;

	socket_send_frame(fd, sending_window_end);

	return 1; // TODO
}



int socket_send_frame(int fd, sequence_num_t sequence_number) {
	int sent_len;

	if (sequence_number > max_sequence_num) {
		fprintf(
			stderr,
			"[%s:%d]: Sequence number provided (%u) exceeds the maximum sequence number (%u)!\n",
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
			"[%s:%d]: Sequence number provided (%u) has already been ACK'd!\n",
			__FILE__,
			__LINE__,
			sequence_number
		);
		return -2;
	}

	sequenced_frames[sequence_number]->state = SENT;

	sent_len = send(
		fd,
		(void*) &sequenced_frames[sequence_number]->frame,
		sizeof(struct frame),
		0
	);

	if (sent_len != sizeof(struct frame)) {
		fprintf(
			stderr,
			"[%s:%d]: Failed to send header of frame with sequence number %u!\n",
			__FILE__,
			__LINE__,
			sequence_number
		);
		exit(EXIT_FAILURE);
	}

	sent_len = send(
		fd,
		(void*) sequenced_frames[sequence_number]->data,
		sequenced_frames[sequence_number]->frame.payload_length,
		0
	);

	if (sent_len != sizeof(struct frame)) {
		fprintf(
			stderr,
			"[%s:%d]: Failed to send payload of frame with sequence number %u!\n",
			__FILE__,
			__LINE__,
			sequence_number
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

	recv_len = recv(fd, (void*) &bframe->frame, sizeof(struct frame), 0);

	if (recv_len <= 0) {
		fprintf(stderr, "[%s:%d]: Connection with receiver closed...\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE); // TODO: Failure?

	} else if (recv_len != sizeof(struct frame)) {
		fprintf(stderr, "[%s:%d]: Received unexpected number of bytes...\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if (bframe->frame.frame_type == FRAME_TYPE_ACK) {
		if (bframe->frame.payload_length != 0) {
			// Ensure an ACK isn't sent with a payload, thus throwing
			// the sender and receiver stream (DGRAM) out of sync
			fprintf(stderr, "[%s:%d]: Received ACK with non-zero byte payload!\n", __FILE__, __LINE__);
		}

		socket_receive_ack(fd, bframe);

	} else if (bframe->frame.frame_type == FRAME_TYPE_DATA) {
		fprintf(stderr, "[%s:%d]: Sender received data frame!\n", __FILE__, __LINE__);

	} else {
		fprintf(
			stderr,
			"[%s:%d]: Sender received unknown frame type (%u)!\n",
			__FILE__,
			__LINE__,
			bframe->frame.frame_type
		);
	}

	return 1; // TODO
}



int socket_receive_ack(int fd, struct buffered_frame *bframe) {

	(void) fd; // TODO

	switch (sequenced_frames[bframe->frame.sequence_number]->state) {
		case UNSENT:
			fprintf(stderr, "[%s:%d]: Sender received ACK for unsent frame!\n", __FILE__, __LINE__);
			break;

		case SENT:
			sequenced_frames[bframe->frame.sequence_number]->state = ACKD;
			unackd_frames--;
			while (sequenced_frames[sending_window_start]->state == ACKD) {
				sending_window_start = (sending_window_start + 1) % (max_sequence_num + 1);
			}
			break;

		case RECVD:
			// TODO
			printf("[%s:%d]: socket_receive_ack() RECVD\n", __FILE__, __LINE__);
			break;

		case ACKD:
			// Reawknowledement of frame
			// TODO
			printf("[%s:%d]: socket_receive_ack() ACKD\n", __FILE__, __LINE__);
			break;
	}
	return 1; // TODO
}



void validate_cli_args(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Usage: %s ReceiverHostname ReceiverPort MaxSendingWindowSize TimeoutSeconds\n\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (strlen(argv[1]) < 2) {
		fprintf(stderr, "Provide a valid server hostname or IP address\n");
		exit(EXIT_FAILURE);
	}

	if (strlen(argv[2]) > 5) {
		fprintf(stderr, "Invalid receiver port number\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < strlen(argv[2]); i++) {
		if (!isdigit(argv[2][i])) {
			fprintf(stderr, "The receiver port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	if (strtoul(argv[2], NULL, 10) > 65535) {
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

}


int main(int argc, char *argv[]) {

	TAILQ_INIT(&send_buffer_head);

	validate_cli_args(argc, argv);


	sending_window_size = strtol(argv[3], NULL, 10);
	if (sending_window_size + 1 >= max_sequence_num) { // TODO: Confirm inequaility
		max_sequence_num = sending_window_size + 1; // TODO: Confirm
	}

	send_timeout = strtol(argv[4], NULL, 10);

	sequenced_frames = (struct buffered_frame**) malloc((sizeof(struct buffered_frame) * max_sequence_num) + 1);
	memset(sequenced_frames, 0, (sizeof(struct buffered_frame) * max_sequence_num) + 1);


	return EXIT_SUCCESS;
}
