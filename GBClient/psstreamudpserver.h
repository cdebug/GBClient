#ifndef PSSTREAMUDPSERVER_H
#define PSSTREAMUDPSERVER_H
#include "psstreamserver.h"
#include <iostream>
#include <string.h>
#include <vector>
#include "common.h"
class H264Decoder;

class PsStreamUdpServer : public PsStreamServer
{
	Q_OBJECT
public:
	PsStreamUdpServer();
	~PsStreamUdpServer();
	int allocReceivePort();
	void executeProcess();
	void stop();
private:
	uint16_t m_seqNo;
};

#endif //PSSTREAMUDPSERVER_H