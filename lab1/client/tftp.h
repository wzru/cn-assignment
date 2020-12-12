#ifndef TFTP_H
#define TFTP_H

#include <cstdint>
#include <string>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

// #define LOG_TIME_FMT "%d/%02d/%02d %02d:%02d:%02d "

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

extern char *host;
extern char *port;
extern uint8 op;
// extern FILE *log_stream;

const int BUF_LEN = 550;
const int READ_LEN = 512;

enum tftp_op { TFTP_RRQ = 1, TFTP_WRQ, TFTP_DATA, TFTP_ACK, TFTP_ERROR };

// converts block number to length-2 string
void s_to_i(char *f, int n)
{
	if (n == 0) {
		f[0] = '0', f[1] = '0', f[2] = '\0';
	} else if (n % 10 > 0 && n / 10 == 0) {
		char c = n + '0';
		f[0] = '0', f[1] = c, f[2] = '\0';
	} else if (n % 100 > 0 && n / 100 == 0) {
		char c2 = (n % 10) + '0';
		char c1 = (n / 10) + '0';
		f[0] = c1, f[1] = c2, f[2] = '\0';
	} else {
		f[0] = '9', f[1] = '9', f[2] = '\0';
	}
}

// makes RRQ pkt
char *make_rrq(char *filename)
{
	char *pkt = (char *)malloc(2 + strlen(filename));
	memset(pkt, 0, sizeof(pkt));
	strcat(pkt, "01"); //opcode
	strcat(pkt, filename);
	return pkt;
}

// makes WRQ pkt
char *make_wrq(char *filename)
{
	char *pkt = (char *)malloc(2 + strlen(filename));
	memset(pkt, 0, sizeof(pkt));
	strcat(pkt, "02"); //opcode
	strcat(pkt, filename);
	return pkt;
}

// makes data pkt
char *make_data_pack(int block, char *data)
{
	char *pkt = (char *)malloc(4 + strlen(data));
	char temp[3];
	s_to_i(temp, block);
	memset(pkt, 0, sizeof(pkt));
	strcat(pkt, "03"); //opcode
	strcat(pkt, temp);
	strcat(pkt, data);
	return pkt;
}

// makes ACK pkt
char *make_ack(char *block)
{
	char *pkt = (char *)malloc(2 + strlen(block));
	memset(pkt, 0, sizeof(pkt));
	strcat(pkt, "04"); //opcode
	strcat(pkt, block);
	return pkt;
}

// makes ERR pkt
char *make_err(char *errcode, char *errmsg)
{
	char *pkt = (char *)malloc(4 + strlen(errmsg));
	memset(pkt, 0, sizeof(pkt));
	strcat(pkt, "05"); //opcode
	strcat(pkt, errcode);
	strcat(pkt, errmsg);
	return pkt;
}

#endif