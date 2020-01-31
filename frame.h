#ifndef __FRAME_H
#define __FRAME_H

#include <time.h>

#define FRAME_TYPE_DATA			0
#define FRAME_TYPE_ACK			1

#define INITIAL_SEQ_NUM			0
#define MAX_SEQ_NUM				15


typedef uint8_t 		frame_type_t;
typedef uint8_t 		sequence_num_t;

enum frame_state {UNSENT, SENT, RECVD, ACKD};

struct frame {
	frame_type_t		frame_type;
	sequence_num_t		sequence_number;
	uint16_t			payload_length;

	// Frame payload follows as payload_length
	// bytes, assumed to be null terminated string.
	// The null terminator is to be counted in payload_length.
};

struct buffered_frame {
	struct				frame frame;
	char				*data;
	enum frame_state	state;
	time_t				sent_time;
};


#endif // __FRAME_H