#ifndef __FRAME_H
#define __FRAME_H

#include <time.h>


// If the frame type is a handshake, the sequence number field
// is used by the sender to communicate the sending window size,
// and thus the largest sequence number is the sending window size.
// This is because 0 is counted as a sequnce number and there is
// (sending window size) + 1 possible sequence numbers
#define FRAME_TYPE_HANDSHAKE				1
#define FRAME_TYPE_DATA						2
#define FRAME_TYPE_DATA_WITH_SEQ_RESET		3
#define FRAME_TYPE_ACK						4

#define INITIAL_SEQ_NUM						0
#define MAX_SEQ_NUM							2


typedef uint8_t 		frame_type_t;
typedef uint16_t 		sequence_num_t;

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