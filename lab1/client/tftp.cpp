#include "tftp.h"
#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

Logger<TextDecorator> logger("tftp-client.txt",
			     "TFTP Client by Shaun(U2018\x31\x34\x38\x36\x37)",
			     true, true);

char *host = "localhost";
char *file;
char *port = "69";
uint8 op = TFTP_RRQ;
ssize_t num;
addrinfo input, *server, *p;
int res, sockfd;
char buf[BUF_LEN], last_recv_msg[BUF_LEN], last_send_ack[BUF_LEN],
	tmp[INET6_ADDRSTRLEN];
sockaddr_storage addr;

inline void init(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	init(argc, argv);
	input.ai_family = AF_UNSPEC;
	input.ai_socktype = SOCK_DGRAM;
	if (res = getaddrinfo(host, port, &input, &server)) {
		logger.Log("getaddrinfo(): %s", gai_strerror(res));
		exit(-1);
	}
	for (p = server; p; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				     p->ai_protocol)) == -1) {
			continue;
		}
		break;
	}
	if (p == NULL) {
		logger.Log("Create socket ERROR");
		exit(-1);
	}
	if (op == TFTP_RRQ) {
		//SENDING RRQ
		char *msg = make_rrq(file);
		cout << sockfd << ":" << msg << endl;
		strcpy(last_recv_msg, "");
		strcpy(last_send_ack, msg);
		if ((num = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr,
				  p->ai_addrlen)) == -1) {
			logger.Log("sendto() ERROR");
			exit(-1);
		}
		logger.Log("Sent %d bytes to %s", num, host);

		FILE *fp = fopen(file, "wb");
		if (fp == NULL) { //ERROR CHECKING
			logger.Log("ERROR creating file: %s", file);
			exit(-1);
		}

		//RECEIVING ACTUAL FILE
		int c_written;
		do {
			//RECEIVING FILE - PACKET DATA
			socklen_t addr_len = sizeof(addr);
			if ((num = recvfrom(sockfd, buf, BUF_LEN - 1, 0,
					    (struct sockaddr *)&addr,
					    &addr_len)) == -1) {
				logger.Log("recvfrom() ERROR");
				exit(-1);
			}
			logger.Log(
				"Get packet from %s\n",
				inet_ntop(addr.ss_family,
					  get_in_addr((struct sockaddr *)&addr),
					  tmp, sizeof(tmp)));
			logger.Log("Recv packet %d bytes", num);
			buf[num] = '\0';
			logger.Log("Recv packet contains \"%s\"\n", buf);

			//CHECKING IF ERROR PACKET
			if (buf[0] == '0' && buf[1] == '5') {
				logger.Log("Get error packet: %s\n", buf);
				exit(-1);
			}

			//SENDING LAST ACK AGAIN - AS IT WAS NOT REACHED
			if (strcmp(buf, last_recv_msg) == 0) {
				sendto(sockfd, last_send_ack,
				       strlen(last_send_ack), 0,
				       (struct sockaddr *)&addr, addr_len);
				continue;
			}

			//WRITING FILE - PACKET DATA
			c_written = strlen(buf + 4);
			fwrite(buf + 4, sizeof(char), c_written, fp);
			strcpy(last_recv_msg, buf);

			//SENDING ACKNOWLEDGEMENT - PACKET DATA
			char block[3];
			strncpy(block, buf + 2, 2);
			block[2] = '\0';
			char *t_msg = make_ack(block);
			if ((num = sendto(sockfd, t_msg, strlen(t_msg), 0,
					  p->ai_addr, p->ai_addrlen)) == -1) {
				perror("CLIENT ACK: sendto");
				exit(-1);
			}
			printf("CLIENT: sent %d bytes\n", num);
			strcpy(last_send_ack, t_msg);
		} while (c_written == READ_LEN);
		printf("NEW FILE: %s SUCCESSFULLY MADE\n", file);
		fclose(fp);
	}
	return 0;
}

inline void init(int argc, char *argv[])
{
	int ch;
	while ((ch = getopt(argc, argv, "h:p:t:")) != -1) {
		switch (ch) {
		case 'h':
			host = strdup(optarg);
			break;
		case 'p':
			port = strdup(optarg);
			break;
		case 't':
			op = (strcasecmp(optarg, "put") ? TFTP_RRQ : TFTP_WRQ);
			break;
		case '?':
			printf("Unknown option: %c\n", optopt);
			exit(-1);
		}
	}
	if (optind >= argc) {
		logger.Log("Need file name.");
		exit(-1);
	}
	file = argv[optind];
	logger.Log("host=%s, port=%s, type=%d, file=%s", host, port, op, file);
}