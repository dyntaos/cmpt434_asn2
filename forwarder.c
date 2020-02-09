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
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>

#include "sender.h"
#include "udp.h"


#define EPOLL_EVENT_COUNT		2


struct buffered_frame **sequenced_frames;
sequence_num_t sending_window_start = INITIAL_SEQ_NUM;
sequence_num_t sending_window_end = INITIAL_SEQ_NUM;
sequence_num_t max_sequence_buffer_num = MAX_SEQ_NUM;
size_t sending_window_size = 1;
size_t unackd_frames = 0;
uint16_t send_timeout = 1;
char *local_port, *receiver_host, *receiver_port;

sequence_num_t next_frame = 0;
bool connected = false;
struct sockaddr sender;
socklen_t sender_sock_len;

uint8_t loss_probability = 0;
bool has_loss_probability = true; // TODO


int socket_forward_frame(int fd, struct buffered_frame *bframe); // TODO HEADER


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



int process_forwarding_frame(int sock_rx_fd, int sock_tx_fd, struct buffered_frame *bframe) {
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

				if (unackd_frames >= sending_window_size - 1) break;

				if (!has_loss_probability) {
					printf("Send ACK? [y/N] ");
					getline(&input, &input_len, stdin);

					if (input[0] == 'y' || input[0] == 'Y') {
						printf("Sending ACK...\n\n");
						transmit_ack(sock_rx_fd, next_frame);
						next_frame++;

					} else {
						printf("ACK withheld for sequence #%d...\n", bframe->frame.sequence_number);
					}

					free(input);
					input = NULL;

					if (socket_forward_frame(sock_tx_fd, bframe) < 0) {
						printf("<DEBUG> ERROR FORWARDING\n");
					}

				} else {
					if (rand() % 100 > loss_probability) {
						printf("Sending ACK...\n");
						transmit_ack(sock_rx_fd, next_frame);
						next_frame++;
					} else {
						printf("Packet loss simulated for sequence #%d\n", bframe->frame.sequence_number);
					}

					if (socket_forward_frame(sock_tx_fd, bframe) < 0) {
						printf("<DEBUG> ERROR FORWARDING\n");
					}
				}

			} else if (bframe->frame.sequence_number == next_frame - 1) {
				printf(
					"==============================================================================\n"
					"Got retransmission of last correctly received in-order message with sequence # %u:\n%s",
					bframe->frame.sequence_number,
					bframe->data
				);
				transmit_ack(sock_rx_fd, next_frame - 1);

			} else if (bframe->frame.sequence_number < next_frame - 1) {
				printf(
					"==============================================================================\n"
					"Got retransmission of previously ACKd message with sequence # %u:\n%s",
					bframe->frame.sequence_number,
					bframe->data
				);
				transmit_ack(sock_rx_fd, next_frame - 1);
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



struct buffered_frame *create_buffered_frame(void *data, size_t data_len) {
	static uint16_t next_sequence_number = INITIAL_SEQ_NUM;
	struct buffered_frame *bframe;

	bframe = (struct buffered_frame*) malloc(sizeof(struct buffered_frame));
	if (bframe == NULL) {
		return NULL;
	}
	bframe->frame.sequence_number = next_sequence_number;
	bframe->frame.frame_type =
		next_sequence_number == max_sequence_buffer_num ?
			FRAME_TYPE_DATA_WITH_SEQ_RESET : FRAME_TYPE_DATA;
	next_sequence_number++;
	bframe->frame.payload_length = data_len;
	bframe->state = UNSENT;
	bframe->data = data;

	return bframe;
}



int socket_forward_frame(int fd, struct buffered_frame *bframe) {

	if (unackd_frames >= sending_window_size - 1) {
		return -1;
	}

	// If the frame in the next slot in the circular buffer was
	// not previously empty, free the data pointer in it.
	if (
		sequenced_frames[sending_window_end % (max_sequence_buffer_num + 1)] != NULL &&
		sequenced_frames[sending_window_end % (max_sequence_buffer_num + 1)]->data != NULL
	) {
		free(sequenced_frames[sending_window_end % (max_sequence_buffer_num + 1)]->data);
		sequenced_frames[sending_window_end % (max_sequence_buffer_num + 1)]->data = NULL;
	}

	sequenced_frames[sending_window_end % (max_sequence_buffer_num + 1)] = bframe;

	unackd_frames++;

	socket_send_frame(fd, sending_window_end);
	sending_window_end++;

	return 1;
}



void socket_send_frame(int fd, sequence_num_t sequence_number) {
	int sent_len;

	sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->state = SENT;
	sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->sent_time = time(NULL);

	sent_len = sendto(
		fd,
		(void*) &sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->frame,
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
		(void*) sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->data,
		sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->frame.payload_length,
		0,
		NULL,
		0
	);

	if (sent_len != sequenced_frames[sequence_number % (max_sequence_buffer_num + 1)]->frame.payload_length) {
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
}



void socket_sender_recv(int fd) {
	struct buffered_frame *bframe;
	int recv_len;

	bframe = (struct buffered_frame*) malloc(sizeof(struct buffered_frame));
	bframe->data = NULL;
	bframe->sent_time = 0;
	bframe->state = RECVD;

	recv_len = recvfrom(fd, (void*) &bframe->frame, sizeof(struct frame), 0, NULL, 0);

	if (recv_len <= 0) {
		fprintf(stderr, "[%s : %d]: Connection with receiver closed...\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);

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
}



void socket_receive_ack(struct buffered_frame *bframe) {

	switch (sequenced_frames[bframe->frame.sequence_number % (max_sequence_buffer_num + 1)]->state) {
		case UNSENT:
			fprintf(stderr, "[%s : %d]: Sender received ACK for unsent frame!\n", __FILE__, __LINE__);
			break;

		case SENT:

			sequenced_frames[bframe->frame.sequence_number % (max_sequence_buffer_num + 1)]->state = ACKD;
			unackd_frames--;
			while (
				sequenced_frames[sending_window_start % (max_sequence_buffer_num + 1)] != NULL &&
				sequenced_frames[sending_window_start % (max_sequence_buffer_num + 1)]->state == ACKD
			) {
				sequenced_frames[sending_window_start % (max_sequence_buffer_num + 1)]->state = UNSENT;
				free(sequenced_frames[sending_window_start % (max_sequence_buffer_num + 1)]->data);
				sequenced_frames[sending_window_start % (max_sequence_buffer_num + 1)]->data = NULL;

				sending_window_start++;
			}
			printf("Received ACK for sequence %u\n", bframe->frame.sequence_number);
			break;

		case RECVD:
			// The sender should not ever see a frame with the state RECV
			fprintf(
				stderr,
				"[%s : %d]: Invalid state: socket_receive_ack() passed frame with state RECVD\n",
				__FILE__,
				__LINE__
			);
			break;

		case ACKD:
			// The sender should not ever see a frame with the state ACKD (even if reACKd),
			// since ACKd frames are freed the first time
			fprintf(
				stderr,
				"[%s : %d]: Invalid state: socket_receive_ack() passed frame with state ACKD\n",
				__FILE__,
				__LINE__
			);
			break;
	}
}



time_t get_timeout(void) {
	time_t min_time = send_timeout, i_time, curr_time;

	if (unackd_frames == 0) {
		return -1;
	}

	curr_time = time(NULL);

	for (int i = sending_window_start; i < sending_window_end; i++) {
		if (sequenced_frames[i % (max_sequence_buffer_num + 1)] == NULL || sequenced_frames[i % (max_sequence_buffer_num + 1)]->state != SENT) continue;
		i_time = curr_time + send_timeout - sequenced_frames[i % (max_sequence_buffer_num + 1)]->sent_time;
		if (i_time < min_time) {
			min_time = i_time;
		}
	}
	return min_time * 1000;
}



void service_timeout(int fd) {
	time_t i_time, curr_time;

	if (unackd_frames == 0) {
		return;
	}

	curr_time = time(NULL);

	for (int i = sending_window_start; i < sending_window_end; i++) {
		if (sequenced_frames[i % (max_sequence_buffer_num + 1)] == NULL || sequenced_frames[i % (max_sequence_buffer_num + 1)]->state != SENT) continue;
		i_time = curr_time + send_timeout - sequenced_frames[i % (max_sequence_buffer_num + 1)]->sent_time;

		if (i_time >= send_timeout) {
			printf("Timeout on sequence #%u, retransmitting frame...\n", sequenced_frames[i % (max_sequence_buffer_num + 1)]->frame.sequence_number);
			socket_send_frame(fd, sequenced_frames[i % (max_sequence_buffer_num + 1)]->frame.sequence_number);
		}
	}
}



int epoll_setup(void) {
	int epollfd;

	epollfd = epoll_create1(0);

	if (epollfd < 0) {
		perror("epoll_create1");
		return -1;
	}
	return epollfd;
}



int epoll_add(int epollfd, int fd) {
	struct epoll_event event;

	event.data.fd = fd;
	event.events = EPOLLIN;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event)) {
		perror("epoll_ctl");
		return -1;
	}
	return 0;
}



void validate_cli_args(int argc, char *argv[]) {
	local_port = argv[1];
	receiver_host = argv[2];
	receiver_port = argv[3];

	if (argc != 6) {
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

	for (size_t i = 0; i < strlen(argv[4]); i++) {
		if (!isdigit(argv[4][i])) {
			fprintf(stderr, "The maximum sending window size provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < strlen(argv[5]); i++) {
		if (!isdigit(argv[5][i])) {
			fprintf(stderr, "The timeout value provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	send_timeout = strtol(argv[5], NULL, 10);

	sending_window_size = strtol(argv[4], NULL, 10);
	if (sending_window_size + 1 >= max_sequence_buffer_num) {
		max_sequence_buffer_num = sending_window_size + 1;
	}
}


int main(int argc, char *argv[]) {
	int sock_rx_fd, sock_tx_fd, epollfd, epoll_count;
	char *input = NULL; //, *queue_text;
	//size_t input_size = 0;
	struct buffered_frame *bframe;
	struct epoll_event events[EPOLL_EVENT_COUNT];
	//int chars_read;


	validate_cli_args(argc, argv);

	sequenced_frames = (struct buffered_frame**) malloc((sizeof(struct buffered_frame) * max_sequence_buffer_num) + 1);

	if (sequenced_frames == NULL) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to allocate memory for circular sending window buffer!\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}
	memset(sequenced_frames, 0, (sizeof(struct buffered_frame) * max_sequence_buffer_num) + 1);

	sock_tx_fd = udp_client_init(receiver_host, receiver_port);
	if (sock_tx_fd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to open sender facing socket...\n",
			__FILE__,
			__LINE__
		);
		exit(EXIT_FAILURE);
	}

	sock_rx_fd = udp_server_init(local_port);
	if (sock_rx_fd < 0) {
		fprintf(
			stderr,
			"[%s : %d]: Failed to open receiver facing socket...\n",
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

	epoll_add(epollfd, sock_rx_fd);
	epoll_add(epollfd, sock_tx_fd);

	printf("Forwarder waiting for inbound frames...\n");

	for (;;) {

		epoll_count = epoll_wait(epollfd, events, EPOLL_EVENT_COUNT, get_timeout());

		if (epoll_count == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (epoll_count == 0) {
			service_timeout(sock_tx_fd);
			continue;
		}

		for (int i = 0; i < epoll_count; i++) {
			/*if (events[i].data.fd == STDIN_FILENO) {
				// STDIN
				chars_read = getline(&input, &input_size, stdin);
				if (chars_read == -1) {
					printf("Read to EOF\n");
					// epoll_wait() will not wait if we are at EOF so delete stdin from the epoll fd
					// This requires kernel 2.6.9 or greater for NULL arg
					epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, NULL);
					continue;
				}

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
				bframe = create_buffered_frame(queue_text, input_size);

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

				// Send until the sending window is full or there are no more messages to send
				while (socket_send_next_frame(sock_tx_fd) >= 0);

			} else*/
			if (events[i].data.fd == sock_tx_fd) {
				// DATA FROM RECEIVER (ACKs)

				socket_sender_recv(sock_tx_fd);
				//while (socket_send_next_frame(sock_tx_fd) >= 0);

			} else if (events[i].data.fd == sock_rx_fd) {
				// DATA FROM SENDER

				bframe = socket_receiver_recv(sock_rx_fd);
				if (process_forwarding_frame(sock_rx_fd, sock_tx_fd, bframe) < 0) {
					fprintf(
						stderr,
						"[%s : %d]: Failed to process received frame!\n",
						__FILE__,
						__LINE__
					);
				}

				free(bframe);
				bframe = NULL;

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
