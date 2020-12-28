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
#include <ctime>

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
const int delay = 10000;
clock_t tbeg, tend;
int siz;

inline void init(int argc, char *argv[]);
inline void help(int argc, char *argv[]);
inline int get();
inline int put();

int main(int argc, char *argv[])
{
	init(argc, argv);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		logger.Log("Server socket could not be created.\n");
		exit(-1);
	}
	freopen("tftp.log", "w", stdout);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	//将点分文本的IP地址转换为二进制网络字节序的IP地址
	inet_pton(AF_INET, host, &(server.sin_addr.s_addr));
	if (op == TFTP_RRQ) {
		if (get()) {
			fprintf(stderr, "%s 下载成功!\n", file);
		} else
			fprintf(stderr, "%s 下载失败!\n", file);
	} else if (op == TFTP_WRQ) {
		if (put()) {
			fprintf(stderr, "%s 上传成功!\n", file);
		} else {
			fprintf(stderr, "%s 上传失败!\n", file);
		}
	}
	if(fp) fclose(fp);
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

inline int get()
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
		fprintf(stderr, "Create file \"%s\" ERROR\n", file);
		logger.Log("Create file \"%s\" ERROR", file);
		return 0;
	}
	// Receive data
	snd_pkt.op = htons(TFTP_ACK);
	tbeg = clock();
	do {
		for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT * PKT_MAX_RXMT;
		     time_wait += delay) {
			// Try receive(non-block receive).
			rcv_size = recvfrom(sockfd, &rcv_pkt, sizeof(TftpPkt),
					    MSG_DONTWAIT, (sockaddr *)&sender,
					    &addr_len);
			printf("recv_size=%d\n", rcv_size);
			if (rcv_size == -1) {
				sendto(sockfd, &snd_pkt, sizeof(TftpPkt), 0,
				       (sockaddr *)&sender, addr_len);
				usleep(delay);
				logger.Log("RESEND : ACK for block #%d...",
					   ntohs(rcv_pkt.block));
			}
			if (rcv_size > 0 && rcv_size < 4) {
				logger.Log("BAD PKT: rcv_size=%d\n", rcv_size);
			}
			if (rcv_size >= 4 && rcv_pkt.op == htons(TFTP_DATA) &&
			    rcv_pkt.block == htons(block)) {
				logger.Log("RECV : block=%d, data_size=%d",
					   ntohs(rcv_pkt.block), rcv_size - 4);
				// Send ACK
				snd_pkt.block = rcv_pkt.block;
				sendto(sockfd, &snd_pkt, sizeof(TftpPkt), 0,
				       (sockaddr *)&sender, addr_len);
				logger.Log("SEND : ACK for block #%d...",
					   ntohs(rcv_pkt.block));
				siz += rcv_size - 4;
				fwrite(rcv_pkt.data, 1, rcv_size - 4, fp);
				break;
			}
			usleep(delay);
		}
		if (time_wait >= PKT_RCV_TIMEOUT * PKT_MAX_RXMT) {
			fprintf(stderr, "WAIT for PKT #%d TIMEOUT!\n", block);
			logger.Log("WAIT for PKT #%d timeout", block);
			return 0;
		}
		block++;
	} while (rcv_size == DATA_SIZE + 4);
	tend = clock();
	auto tim = (double)(tend - tbeg) / CLOCKS_PER_SEC;
	fprintf(stderr, "file-size=%d byte, time=%.3lf s\n", siz, tim);
	fprintf(stderr, "throughput=%.3lf KiB/s\n", (double)siz / 1024 / tim);
	return 1;
}

inline int put()
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
	for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT; time_wait += delay) {
		// Try receive(non-block receive)
		rcv_size = recvfrom(sockfd, &rcv_pkt, sizeof(TftpPkt),
				    MSG_DONTWAIT, (struct sockaddr *)&sender,
				    &addr_len);
		if (rcv_size > 0 && rcv_size < 4) {
			logger.Log("BAD PKT: rcv_size=%d", rcv_size);
		}
		if (rcv_size >= 4 && rcv_pkt.op == htons(TFTP_ACK) &&
		    rcv_pkt.block == htons(0)) {
			break;
		}
		usleep(delay);
	}
	if (time_wait >= PKT_RCV_TIMEOUT) {
		logger.Log("Could not receive from server");
		return 0;
	}
	if (!strcmp(mode, "netascii"))
		fp = fopen(file, "r");
	else
		fp = fopen(file, "rb");
	if (fp == NULL) {
		logger.Log("File not exists!");
		fprintf(stderr, "File not exists!\n");
		return 0;
	}
	int snd_size = 0, rxmt;
	uint16 block = 1;
	snd_pkt.op = htons(TFTP_DATA);
	// Send data
	tbeg = clock();
	do {
		memset(snd_pkt.data, 0, sizeof(snd_pkt.data));
		snd_pkt.block = htons(block);
		siz += DATA_SIZE;
		snd_size = fread(snd_pkt.data, 1, DATA_SIZE, fp);
		for (rxmt = 0; rxmt < PKT_MAX_RXMT; ++rxmt) {
			sendto(sockfd, &snd_pkt, snd_size + 4, 0,
			       (sockaddr *)&sender, addr_len);
			logger.Log("SEND : PKT #%d", block);
			// Wait for ACK.
			for (time_wait = 0; time_wait < PKT_RCV_TIMEOUT;
			     time_wait += delay) {
				// Try receive(non-block receive).
				rcv_size =
					recvfrom(sockfd, &rcv_pkt,
						 sizeof(TftpPkt), MSG_DONTWAIT,
						 (sockaddr *)&sender,
						 &addr_len);
				if (rcv_size > 0 && rcv_size < 4) {
					logger.Log("BAD PKT: rcv_size=%d",
						   rcv_size);
				}
				if (rcv_size >= 4 &&
				    rcv_pkt.op == htons(TFTP_ACK) &&
				    rcv_pkt.block == htons(block)) {
					break;
				}
				usleep(delay);
			}
			if (time_wait < PKT_RCV_TIMEOUT) {
				break; // Send success
			} else {
				continue; // Retransmission
			}
		}
		if (rxmt >= PKT_MAX_RXMT) {
			fprintf(stderr, "Could not receive from server!\n");
			logger.Log("Could not receive from server");
			return 0;
		}
		block++;
	} while (snd_size == DATA_SIZE);
	tend = clock();
	auto tim = (double)(tend - tbeg) / CLOCKS_PER_SEC;
	fprintf(stderr, "file-size=%d byte, time=%.3lf s\n", siz, tim);
	fprintf(stderr, "throughput=%.3lf KiB/s\n", (double)siz / 1024 / tim);
	return 1;
}