#include "psstreamudpserver.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")  
#include "h264decoder.h"
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#define MAX_BUFF_SIZE 4096

PsStreamUdpServer::PsStreamUdpServer()
{
	m_seqNo = 0xffff;
}

PsStreamUdpServer::~PsStreamUdpServer()
{
	while (m_bRunning);
}

int PsStreamUdpServer::allocReceivePort()
{
	for (int i = PORT_ALLOC_BEGIN; i < PORT_ALLOC_END; ++i)
	{
		m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (m_sockfd == SOCKET_ERROR)
		{
			printf("Socket init error\n");
			continue;
		}
		struct sockaddr_in addr_s;
		memset(&addr_s, 0, sizeof(addr_s));
		addr_s.sin_family = AF_INET;
		addr_s.sin_port = htons(i);
		addr_s.sin_addr.s_addr = INADDR_ANY;
		/* °ó¶¨socket */
		if (bind(m_sockfd, (struct sockaddr *)&addr_s, sizeof(addr_s)) == SOCKET_ERROR)
		{
			perror("bind error:\n");
			closesocket(m_sockfd);
			continue;
		}
		return i;
	}
	return -1;
}

void PsStreamUdpServer::executeProcess()
{
	m_bRunning = true;
	m_bStopFlag = false;
	struct sockaddr_in addr_c;
	memset(&addr_c, 0, sizeof(addr_c));
	char ipbuf[20];
	int n, len = sizeof(addr_c);
	char recv_buf[MAX_BUFF_SIZE];
	while (1)
	{
		if (m_bStopFlag)
			break;
		n = recvfrom(m_sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&addr_c, &len);
		if (n == SOCKET_ERROR)
		{
			perror("recvfrom error:\n");
			break;
		}
		if (n > 12)
		{
			uint32_t ssrc;
			memcpy((uint8_t*)&ssrc, (uint8_t*)recv_buf + 8, 4);
			ssrc = htonl(ssrc);
			uint16_t seq;
			memcpy((uint8_t*)&seq, (uint8_t*)recv_buf + 2, 2);
			seq = htons(seq);
			if ((uint16_t)(m_seqNo + 1) == seq)
			{
				RtpSource* rtp = getRtpSource(ssrc);
				memcpy(rtp->data, (uint8_t*)recv_buf, n);
				rtp->len = n;
				parserRtpData(rtp);
				emit packetInfo(inet_ntoa(addr_c.sin_addr), ntohs(addr_c.sin_port), ssrc);
			}/*
			else
				printf("seq %d!=%d+1\n", seq, m_seqNo);*/
			m_seqNo = seq;
		}
	}
	closesocket(m_sockfd);
	m_bRunning = false;
}

void PsStreamUdpServer::stop()
{
	for (auto it : m_rtpSources)
		delete it;
	closesocket(m_sockfd);
}