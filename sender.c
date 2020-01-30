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
#include <sys/queue.h>


TAILQ_HEAD(send_buffer_head_t, send_buffer_item) send_buffer_head;

struct send_buffer_head_t *send_buffer_headp;
size_t send_buffer_size = 0;
struct send_buffer_item {
	char *text;
	TAILQ_ENTRY(send_buffer_item) next_item;
};


void validate_cli_args(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Usage: %s ReceiverHostname ReceiverPort MaxSendingWindowSize TimeoutSeconds\n\n");
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

	for (size_t i = 0; i < strlen(argv[4]); i++) {
		if (!isdigit(argv[4][i])) {
			fprintf(stderr, "The timeout value provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}
}


void send_enqueue(char *text) {
	struct send_buffer_item *sbi;
	sbi = (struct send_buffer_item*) malloc(sizeof(struct send_buffer_item));
	sbi->text = text;
	TAILQ_INSERT_HEAD(&send_buffer_head, sbi, next_item);
	send_buffer_size++;
}


char *send_dequeue(void) {
	struct send_buffer_item *sbi;
	char *text;

	sbi = TAILQ_LAST(&send_buffer_head, send_buffer_head_t);
	TAILQ_REMOVE(&send_buffer_head, sbi, next_item);
	send_buffer_size--;
	text = sbi->text;
	free(sbi);
	return text;
}


int main(int argc, char *argv[]) {
	send_buffer_head = TAILQ_HEAD_INITIALIZER(send_buffer_head);
	TAILQ_INIT(&send_buffer_head);

	validate_cli_args(argc, argv);



	return EXIT_SUCCESS;
}
