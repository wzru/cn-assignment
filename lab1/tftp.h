#ifndef TFTP_H
#define TFTP_H

#include <cstdint>
#include <string>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <netinet/in.h>

// #define LOG_TIME_FMT "%d/%02d/%02d %02d:%02d:%02d "

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

extern char *host;
extern uint16 port;
extern uint8 op;
extern char *mode;

const int BUF_LEN = 550;
const int DATA_SIZE = 512;

enum tftp_op {
	TFTP_RRQ = 1,
	TFTP_WRQ,
	TFTP_DATA,
	TFTP_ACK,
	TFTP_ERROR,
	TFTP_LIST
};

// Max packet retransmission.
const int PKT_MAX_RXMT = 10;
// usecond
const int PKT_SND_TIMEOUT = 12 * 1000 * 1000;
const int PKT_RCV_TIMEOUT = 3 * 1000 * 1000;

struct TftpPkt {
	uint16 op;
	union {
		uint16 code;
		uint16 block;
		// For a RRQ and WRQ TFTP packet
		char filename[2];
	};
	char data[DATA_SIZE];
};

struct TftpxReq {
	uint32 size;
	sockaddr_in c;
	TftpPkt pkt;
};

#endif