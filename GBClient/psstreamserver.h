#ifndef PSSTREAMSERVER_H
#define PSSTREAMSERVER_H
#include <QObject>
#include <iostream>
#include <string.h>
#include <vector>
#include "common.h"
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")  
#include "h264decoder.h"
#include "aacdecoder.h"
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
class H264Decoder;
class AACDecoder;
#define PORT_ALLOC_BEGIN 20000
#define PORT_ALLOC_END 20500

class PsStreamServer : public QObject
{
	Q_OBJECT
public:
	PsStreamServer();
	virtual ~PsStreamServer();
	virtual int allocReceivePort() = 0;
	virtual void executeProcess() = 0;
	virtual void stop() = 0;
	bool sourceExists(uint32_t);
	bool isRunning();
	H264Decoder* videoDecoder();
	AACDecoder* audioDecoder();
protected:
	RtpSource* getRtpSource(uint32_t);
	void parserRtpData(RtpSource*);
	int getPayloadType(RtpSource*);
	void accessPsPacket(RtpSource*, int);
	void resolvePsPacket(RtpSource*, int);
	void resolvePesPacket(RtpSource*, int);
	void getVideoFrame(RtpSource*, int);
	void getAudioFrame(RtpSource*, int);
	void appendFrameData(RtpSource*, int);
	void writeVideoFrame(RtpSource*);
	void writeAudioFrame(RtpSource*);

	std::vector<RtpSource*> m_rtpSources;
	H264Decoder* m_videoDecoder;
	AACDecoder* m_audioDecoder;
	SOCKET m_sockfd;
	bool m_bStopFlag;
	bool m_bRunning;
signals:
	void packetInfo(QString, int, quint32);
};

#endif //PSSTREAMSERVER_H