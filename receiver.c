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
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "frame.h"
#include "udp.h"


char *local_port;

sequence_num_t next_frame = 0;
// This is set when the receiver gets a frame with the type of FRAME_TYPE_DATA_WITH_SEQ_RESET
uint16_t max_sequence_buffer_num = 0;

bool connected = false;
struct sockaddr sender;
socklen_t sender_sock_len;

uint8_t loss_probability = 0;
bool has_loss_probability = false;


struct buffered_frame *socket_receiver_recv(int sockfd) {
	struct buffered_frame *bframe;
	int recv_len;

	bframe = (struct buffered_frame*) malloc(sizeof(struct buffered_frame));
	bframe->data = NULL;
	bframe->sent_time = 0;
	bframe->state = RECVD;

	recv_len = recvfrom(sockfd, (void*) &bframe->frame, sizeof(struct frame), 0, &sender, &sender_sock_len);

	if (recv_len <= 0) {
		fprintf(stderr, "[%s : %d]: Connection with sender closed...\n", __FILE__, __LINE__);
		free(bframe);
		return NULL;

	} else if (recv_len != sizeof(struct frame)) {
		fprintf(stderr, "[%s : %d]: Received unexpected number of bytes...\n", __FILE__, __LINE__);
		free(bframe);
		return NULL;
	}

	if (bframe->frame.frame_type == FRAME_TYPE_ACK) {
		fprintf(
			stderr,
			"[%s : %d]: Receiver was sent an ACK frame!\n\tFrame Type: %u\n\tSequence #%u\n\tData: %s\n\n",
			__FILE__,
			__LINE__,
			bframe->frame.frame_type,
			bframe->frame.sequence_number,
			bframe->data
		);

	} else if (bframe->frame.frame_type == FRAME_TYPE_DATA || bframe->frame.frame_type == FRAME_TYPE_DATA_WITH_SEQ_RESET) {

		if (bframe->frame.frame_type == FRAME_TYPE_DATA_WITH_SEQ_RESET) {
			max_sequence_buffer_num = bframe->frame.sequence_number;
		}

		bframe->data = (char*) malloc(bframe->frame.payload_length);
		if (bframe->data == NULL) {
			fprintf(stderr, "[%s : %d]: Failed to allocate data buffer!\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}

		recv_len = recvfrom(sockfd, (void*) bframe->data, bframe->frame.payload_length, 0, &sender, &sender_sock_len);

		if (recv_len < 0 || recv_len != bframe->frame.payload_length) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to receive the expected number of bytes from socket!\n",
				__FILE__,
				__LINE__
			);
			exit(EXIT_FAILURE);
		}

		return bframe;

	} else {
		fprintf(
			stderr,
			"[%s : %d]: Sender received unknown frame type (%u)!\n",
			__FILE__,
			__LINE__,
			bframe->frame.frame_type
		);
	}

	free(bframe);
	return NULL;
}



void transmit_ack(int sockfd, sequence_num_t ack_sequence) {
	struct frame frame;
	int send_len;

	frame.frame_type = FRAME_TYPE_ACK;
	frame.payload_length = 0;
	frame.sequence_number = ack_sequence;

	send_len = sendto(sockfd, (void*) &frame, sizeof(frame), 0, &sender, sender_sock_len);

	if (send_len != sizeof(frame)) {
		perror("ack");
		fprintf(
			stderr,
			"[%s : %d]: Failed to send ACK! Sent %d bytes\n",
			__FILE__,
			__LINE__,
			send_len
		);
		exit(EXIT_FAILURE);
	}
}



int process_received_frame(int sockfd, struct buffered_frame *bframe) {
	char *input = NULL;
	size_t input_len = 0;

	if (bframe->frame.frame_type != FRAME_TYPE_DATA && bframe->frame.frame_type != FRAME_TYPE_DATA_WITH_SEQ_RESET) {
		fprintf(
			stderr,
			"[%s : %d]: Error: process_received_frame() may only be passed data frames!\n",
			__FILE__,
			__LINE__
		);
		return -1;
	}

	switch (bframe->state) {
		case UNSENT:
			fprintf(
				stderr,
				"[%s : %d]: Error: process_received_frame() received frame with UNSENT state!\n",
				__FILE__,
				__LINE__
			);
			return -1;
			break;

		case SENT:
			fprintf(
				stderr,
				"[%s : %d]: Error: process_received_frame() received frame with SENT state!\n",
				__FILE__,
				__LINE__
			);
			return -1;
			break;

		case RECVD:
			if (next_frame == bframe->frame.sequence_number) {
				printf(
					"==============================================================================\n"
					"Got in-order frame with sequence #%u:\n%s\n",
					bframe->frame.sequence_number,
					bframe->data
				);

				if (!has_loss_probability) {
					printf("Send ACK? [y/N] ");
					getline(&input, &input_len, stdin);

					if (input[0] == 'y' || input[0] == 'Y') {
						printf("Sending ACK...\n\n");
						transmit_ack(sockfd, next_frame);
						next_frame++;

					} else {
						printf("ACK withheld for sequence #%d...\n", bframe->frame.sequence_number);
					}

					free(input);
					input = NULL;

				} else {
					if ((rand() % 100) + 1 > loss_probability) {
						printf("Sending ACK...\n");
						transmit_ack(sockfd, next_frame);
						next_frame++;
					} else {
						printf("Packet loss simulated for sequence #%d\n", bframe->frame.sequence_number);
					}
				}

			} else if (bframe->frame.sequence_number == next_frame - 1) {
				printf(
					"==============================================================================\n"
					"Got retransmission of last correctly received in-order message with sequence # %u:\n%s",
					bframe->frame.sequence_number,
					bframe->data
				);
				transmit_ack(sockfd, next_frame - 1);

			} else if (bframe->frame.sequence_number < next_frame - 1) {
				printf(
					"==============================================================================\n"
					"Got retransmission of previously ACKd message with sequence # %u:\n%s",
					bframe->frame.sequence_number,
					bframe->data
				);
				transmit_ack(sockfd, next_frame - 1);
			}
			break;

		case ACKD:
			fprintf(
				stderr,
				"[%s : %d]: Invalid state: process_received_frame(): passed frame with state ACKD!\n",
				__FILE__,
				__LINE__
			);
			return -1;
			break;

		default:
			fprintf(
				stderr,
				"[%s : %d]: Invalid state: process_received_frame(): passed frame with unkown state (%u)!\n",
				__FILE__,
				__LINE__,
				bframe->state
			);
			return -1;
			break;
	}
	return 0;
}



void validate_cli_args(int argc, char *argv[]) {
	local_port = argv[1];

	if (argc != 2 && argc != 3) {
		printf("Usage: %s LocalPort [PacketLoss%%]\n\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (strlen(local_port) > 5) {
		fprintf(stderr, "Invalid receiver port number\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < strlen(local_port); i++) {
		if (!isdigit(local_port[i])) {
			fprintf(stderr, "The receiver port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	if (strtoul(local_port, NULL, 10) > 65535) {
		fprintf(stderr, "Receiver port number must be between 0 to 65535\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 3) {
		if (strlen(argv[2]) > 2) {
			fprintf(stderr, "Invalid loss probabilty (use values from 0 - 99)\n");
			exit(EXIT_FAILURE);
		}

		for (size_t i = 0; i < strlen(argv[2]); i++) {
			if (!isdigit(argv[2][i])) {
				fprintf(stderr, "The loss probability provided must be an integer\n");
				exit(EXIT_FAILURE);
			}
		}

		loss_probability = strtol(argv[2], NULL, 10);
		has_loss_probability = true;
		srand(time(NULL));
	}
}



int main(int argc, char *argv[]) {
	int sockfd;
	struct buffered_frame *bframe;

	validate_cli_args(argc, argv);

	sockfd = udp_server_init(local_port);

	if (sockfd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to create open socket...\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}

	printf("Receiver waiting for inbound frames...\n");
	for (;;) {

		bframe = socket_receiver_recv(sockfd);
		if (process_received_frame(sockfd, bframe) < 0) {
			fprintf(
				stderr,
				"[%s : %d]: Failed to process received frame!\n",
				__FILE__,
				__LINE__
			);
		}

		free(bframe);
		bframe = NULL;
	}

	return EXIT_SUCCESS;
}
