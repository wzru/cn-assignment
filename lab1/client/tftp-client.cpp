#include "tftp.h"
#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

using namespace std;

Logger<TextDecorator> logger("tftp-client.txt",
			     "TFTP Client by Shaun(U2018\x31\x34\x38\x36\x37)",
			     true, true);

char *host = NULL;
char *file = NULL;
char *mode = NULL;
uint16 port = 69;
uint8 op = TFTP_RRQ;
FILE *fp;
sockaddr_in server;
int res, sockfd;
socklen_t addr_len = sizeof(sockaddr_in);

inline void init(int argc, char *argv[]);
inline void help(int argc, char *argv[]);
inline void get();
inline void put();

int main(int argc, char *argv[])
{
	init(argc, argv);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		logger.Log("Server socket could not be created.\n");
		exit(-1);
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	//将点分文本的IP地址转换为二进制网络字节序的IP地址
	inet_pton(AF_INET, host, &(server.sin_addr.s_addr));
	if (op == TFTP_RRQ)
		get();
	else if (op == TFTP_WRQ)
		put();
	fclose(fp);
	return 0;
}

inline void init(int argc, char *argv[])
{
	int ch;
	while ((ch = getopt(argc, argv, "h:p:m:")) != -1) {
		switch (ch) {
		case 'h':
			host = strdup(optarg);
			break;
		case 'm':
			mode = strdup(optarg);
			if (strcmp(mode, "octet") && strcmp(mode, "netascii")) {
				logger.Log("Unknown mode: %s", mode);
				help(argc, argv);
				exit(-1);
			}
			break;
		case 'p':
			sscanf(optarg, "%hd", &port);
			break;
		case '?':
			logger.Log("Unknown option: %c", optopt);
			exit(-1);
		}
	}
	if (optind + 2 > argc) {
		help(argc, argv);
		exit(-1);
	}
	if (!host) {
		host = strdup("127.0.0.1");
	}
	if (!mode) {
		mode = strdup("netascii");
	}
	if (!strcasecmp(argv[optind], "get")) {
		op = TFTP_RRQ;
	} else if (!strcasecmp(argv[optind], "put")) {
		op = TFTP_WRQ;
	} else {
		help(argc, argv);
		exit(-1);
	}
	file = argv[optind + 1];
	logger.Log("host=%s, port=%d, type=%s, file=%s", host, port,
		   argv[optind], file);
}

inline void help(int argc, char *argv[])
{
	printf("usage: %s [-h <ip>] [-p <port>] [-m (octet|netascii)] <get|put> <filename>\n",
	       argv[0]);
}

inline void get()
{
	TftpPkt snd_pkt, rcv_pkt;
	sockaddr_in sender;
	int rcv_size = 0, time_wait;
	uint16 block = 1;
	// Send request
	snd_pkt.op = htons(TFTP_RRQ);
	sprintf(snd_pkt.filename, "%s%c%s%c%d%c", file, 0, mode, 0, DATA_SIZE,
		0);
	sendto(sockfd, &snd_pkt, sizeof(struct TftpPkt), 0,
	       (struct sockaddr *)&server, addr_len);
	if (!strcmp(mode, "netascii"))
		fp = fopen(file, "w");
	else
		fp = fopen(file, "wb");
	if (fp == NULL) {
		logger.Log("Create file \"%s\" ERROR", file);
		return;
	}
	// Receive data
	snd_pkt.op = htons(TFTP_ACK);
	do {
		for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT * PKT_MAX_RXMT;
		     time_wait += 10000) {
			// Try receive(Nonblock receive).
			rcv_size = recvfrom(sockfd, &rcv_pkt, sizeof(TftpPkt),
					    MSG_DONTWAIT, (sockaddr *)&sender,
					    &addr_len);
			if (rcv_size > 0 && rcv_size < 4) {
				logger.Log("BAD pkt: rcv_size=%d\n", rcv_size);
			}
			if (rcv_size >= 4 && rcv_pkt.op == htons(TFTP_DATA) &&
			    rcv_pkt.block == htons(block)) {
				logger.Log("Recv : block=%d, data_size=%d",
					   ntohs(rcv_pkt.block), rcv_size - 4);
				// Send ACK
				snd_pkt.block = rcv_pkt.block;
				sendto(sockfd, &snd_pkt, sizeof(TftpPkt), 0,
				       (sockaddr *)&sender, addr_len);
				logger.Log("Sending ACK for block #%d...",
					   ntohs(rcv_pkt.block));
				fwrite(rcv_pkt.data, 1, rcv_size - 4, fp);
				break;
			}
			usleep(10000);
		}
		if (time_wait >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT) {
			logger.Log("Wait for PKT #%d timeout", block);
			return;
		}
		block++;
	} while (rcv_size == DATA_SIZE + 4);
}

inline void put()
{
	TftpPkt rcv_pkt, snd_pkt;
	sockaddr_in sender;
	int rcv_size = 0, time_wait;
	// Send request and wait for ACK#0
	snd_pkt.op = htons(TFTP_WRQ);
	sprintf(snd_pkt.filename, "%s%c%s%c%d%c", file, 0, mode, 0, DATA_SIZE,
		0);
	sendto(sockfd, &snd_pkt, sizeof(TftpPkt), 0, (struct sockaddr *)&server,
	       addr_len);
	for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT; time_wait += 20000) {
		// Try receive(Nonblock receive)
		rcv_size = recvfrom(sockfd, &rcv_pkt, sizeof(TftpPkt),
				    MSG_DONTWAIT, (struct sockaddr *)&sender,
				    &addr_len);
		if (rcv_size > 0 && rcv_size < 4) {
			logger.Log("BAD pkt: rcv_size=%d", rcv_size);
		}
		if (rcv_size >= 4 && rcv_pkt.op == htons(TFTP_ACK) &&
		    rcv_pkt.block == htons(0)) {
			break;
		}
		usleep(20000);
	}
	if (time_wait >= PKT_RCV_TIMEOUT) {
		logger.Log("Could not receive from server");
		return;
	}
	if (!strcmp(mode, "netascii"))
		fp = fopen(file, "r");
	else
		fp = fopen(file, "rb");
	if (fp == NULL) {
		logger.Log("File not exists!");
		return;
	}
	int snd_size = 0, rxmt;
	uint16 block = 1;
	snd_pkt.op = htons(TFTP_DATA);
	// Send data
	do {
		memset(snd_pkt.data, 0, sizeof(snd_pkt.data));
		snd_pkt.block = htons(block);
		snd_size = fread(snd_pkt.data, 1, DATA_SIZE, fp);
		for (rxmt = 0; rxmt < PKT_MAX_RXMT; ++rxmt) {
			sendto(sockfd, &snd_pkt, snd_size + 4, 0,
			       (sockaddr *)&sender, addr_len);
			logger.Log("Send %d", block);
			// Wait for ACK.
			for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT;
			     time_wait += 20000) {
				// Try receive(Nonblock receive).
				rcv_size =
					recvfrom(sockfd, &rcv_pkt,
						 sizeof(TftpPkt), MSG_DONTWAIT,
						 (sockaddr *)&sender,
						 &addr_len);
				if (rcv_size > 0 && rcv_size < 4) {
					logger.Log("BAD pkt: rcv_size=%d",
						   rcv_size);
				}
				if (rcv_size >= 4 &&
				    rcv_pkt.op == htons(TFTP_ACK) &&
				    rcv_pkt.block == htons(block)) {
					break;
				}
				usleep(20000);
			}
			if (time_wait < PKT_RCV_TIMEOUT) {
				// Send success
				break;
			} else {
				// Retransmission
				continue;
			}
		}
		if (rxmt >= PKT_MAX_RXMT) {
			logger.Log("Could not receive from server");
			return;
		}
		block++;
	} while (snd_size == DATA_SIZE);
	logger.Log("Send file end");
}