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

#include "frame.h"


void validate_cli_args(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s LocalPort\n\n");
		exit(EXIT_FAILURE);
	}

	if (strlen(argv[1]) > 5) {
		fprintf(stderr, "Invalid receiver port number\n");
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < strlen(argv[1]); i++) {
		if (!isdigit(argv[1][i])) {
			fprintf(stderr, "The receiver port number provided must be numeric\n");
			exit(EXIT_FAILURE);
		}
	}

	if (strtoul(argv[1], NULL, 10) > 65535) {
		fprintf(stderr, "Receiver port number must be between 0 to 65535\n");
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char *argv[]) {


	validate_cli_args(argc, argv);

	return EXIT_SUCCESS;
}

	/*
	if (bframe->frame.frame_type == FRAME_TYPE_DATA) {

		bframe->data = (char*) malloc(bframe->frame.payload_length);
		recv_len = recv(fd, (void*) bframe->data, sizeof(bframe->frame.payload_length), 0);

		if (recv_len <= 0) {
			fprintf(stderr, "[%s:%d]: Connection with receiver closed...\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE); // TODO: Failure?

		} else if (recv_len != bframe->frame.payload_length) {
			fprintf(stderr, "[%s:%d]: Received unexpected number of bytes...\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}

	}
	*/