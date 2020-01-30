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