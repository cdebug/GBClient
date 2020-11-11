#include "psstreamtcpserver.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_BUFF_SIZE 4096

PsStreamTcpServer::PsStreamTcpServer()
{
}


PsStreamTcpServer::~PsStreamTcpServer()
{
	while (m_bRunning);
}

int PsStreamTcpServer::allocReceivePort()
{
	for (int i = PORT_ALLOC_BEGIN; i < PORT_ALLOC_END; ++i)
	{
		m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
		/* 绑定socket */
		if (bind(m_sockfd, (struct sockaddr *)&addr_s, sizeof(addr_s)) == SOCKET_ERROR)
		{
			perror("bind error:\n");
			closesocket(m_sockfd);
			continue;
		}
		if (listen(m_sockfd, 10) == SOCKET_ERROR)
		{
			perror("listen error:\n");
			closesocket(m_sockfd);
			continue;
		}
		m_port = i;
		return i;
	}
	return -1;
}

void PsStreamTcpServer::executeProcess()
{
	m_bRunning = true;
	m_bStopFlag = false;
	struct sockaddr_in addr_c;
	memset(&addr_c, 0, sizeof(addr_c));
	char ipbuf[20];
	int n, len = sizeof(addr_c);
	uint32_t packeLen = 0, packetSize = 0;
	char recv_buf[MAX_BUFF_SIZE];
	uint8_t rtp_packet[MAX_BUFF_SIZE];
	while (1)
	{
		printf("wait for connection\n");
		m_connfd = accept(m_sockfd, (struct sockaddr *)&addr_c, &len);
		printf("got connection\n");
		if (m_connfd == SOCKET_ERROR)
			break;
		m_ip = inet_ntoa(addr_c.sin_addr);
		m_port = ntohs(addr_c.sin_port);
		while (1)
		{
			if (m_bStopFlag)
				break;
			n = recv(m_connfd, recv_buf, sizeof(recv_buf), 0);
			if (n == SOCKET_ERROR)
			{
				perror("recv error:\n");
				break;
			}
			uint8_t* tmp = (uint8_t*)recv_buf;
			if (n>0 && packetSize > 0)
			{
				int remainSize = packetSize - packeLen;
				if (n >= remainSize)
				{
					memcpy(rtp_packet + packeLen, tmp, remainSize);
					packeLen = packetSize;
					parseWholeRtpPacket(rtp_packet, packeLen);
					tmp += remainSize;
					n -= remainSize;
					packetSize = 0;
					packeLen = 0;
				}
				else
				{
					memcpy(rtp_packet + packeLen, tmp, n);
					packeLen += n;
					tmp += n;
					n = 0;
				}
			}
			while (n >0 && packetSize == 0)
			{
				//前两个是rtp大小
				packetSize = (tmp[0] << 8) | tmp[1];
				tmp += 2;
				n -= 2;
				if (n >= packetSize)
				{
					memcpy(rtp_packet, tmp, packetSize);
					packeLen = packetSize;
					parseWholeRtpPacket(rtp_packet, packeLen);
					tmp += packeLen;
					n -= packeLen;
					packetSize = 0;
					packeLen = 0;
				}
				else
				{
					memcpy(rtp_packet, tmp, n);
					packeLen = n;
					tmp += packeLen;
					n -= packeLen;
				}
			}
		}
		closesocket(m_connfd);
		break;
	}
	closesocket(m_sockfd);
	m_bRunning = false;
}

void PsStreamTcpServer::parseWholeRtpPacket(uint8_t* data, int len)
{
	if (len > 12)
	{
		uint32_t ssrc;
		memcpy((uint8_t*)&ssrc, data + 8, 4);
		ssrc = htonl(ssrc);
		RtpSource* rtp = getRtpSource(ssrc);
		memcpy(rtp->data, data, len);
		rtp->len = len;
		parserRtpData(rtp);
		emit packetInfo(m_ip.c_str(), m_port, ssrc);
	}
}

void PsStreamTcpServer::stop()
{
	for (auto it : m_rtpSources)
		delete it;
	closesocket(m_connfd);
	closesocket(m_sockfd);
}