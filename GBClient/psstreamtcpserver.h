#ifndef PSSTREAMTCPSERVER_H
#define PSSTREAMTCPSERVER_H
#include "psstreamserver.h"
#include <iostream>

class PsStreamTcpServer : public PsStreamServer
{
	Q_OBJECT
public:
	PsStreamTcpServer();
	~PsStreamTcpServer();
	int allocReceivePort();
	void executeProcess();
	void parseWholeRtpPacket(uint8_t*, int);
	void stop();
private:
	std::string m_ip;
	int m_port;
	SOCKET m_connfd;
};

#endif //PSSTREAMTCPSERVER_H