#include "psstreamserver.h"
#include <stdio.h>
#include <stdlib.h>
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
FILE *save_ps;
PsStreamServer::PsStreamServer()
{
	m_videoDecoder = new H264Decoder;
	m_videoDecoder->init();
	m_audioDecoder = new AACDecoder;
	m_audioDecoder->aac_decoder_create(44100, 1, 0);
	m_bRunning = false;
	save_ps = fopen("save.ps", "wb");
}

PsStreamServer::~PsStreamServer()
{
}

void PsStreamServer::stop()
{
	for (auto it : m_rtpSources)
		delete it;
	closesocket(m_sockfd);
}

RtpSource* PsStreamServer::getRtpSource(uint32_t ssrc)
{
	RtpSource* rtp;
	for (int i = 0; i < m_rtpSources.size(); ++i)
	{
		rtp = m_rtpSources.at(i);
		if (rtp->ssrc == ssrc)
			return rtp;
	}
	printf("new rtp source:%d\n", ssrc);
	rtp = new RtpSource;
	rtp->ssrc = ssrc;
	m_rtpSources.push_back(rtp);
	return rtp;
}

bool PsStreamServer::sourceExists(uint32_t ssrc)
{
	for (int i = 0; i < m_rtpSources.size(); ++i)
	{
		RtpSource* rtp = m_rtpSources.at(i);
		if (rtp->ssrc == ssrc)
			return true;
	}
	return false;
}

bool PsStreamServer::isRunning()
{
	return m_bRunning;
}

H264Decoder* PsStreamServer::videoDecoder()
{
	return m_videoDecoder;
}

AACDecoder* PsStreamServer::audioDecoder()
{
	return m_audioDecoder;
}

int PsStreamServer::getPayloadType(RtpSource* rtp)
{
	int ret = 0;
	if (rtp->len > 12)
		ret = rtp->data[1] & 0x7F;
	return ret;
}

void PsStreamServer::parserRtpData(RtpSource* rtp)
{
	if (getPayloadType(rtp) == 96)
	{
		fwrite(rtp->data+12, 1, rtp->len-12, save_ps);
		accessPsPacket(rtp, 12);
	}
}

void PsStreamServer::accessPsPacket(RtpSource* rtp, int dataPos)
{
	//查找ps头 0x000001BA
	if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
		rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xba)
	{
		rtp->isFrameStart = true;
		resolvePsPacket(rtp, dataPos);
	}
	else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
		rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xe0)//0x000001e0 视频流
	{
		rtp->isFrameStart = false;
		resolvePesPacket(rtp, dataPos);
	}
	else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
		rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xc0)//0x000001c0 音频流
	{
		resolvePesPacket(rtp, dataPos);
	}
	else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
		rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xbd)//私有数据
	{
	}
	else  //当然如果开头不是0x000001BA,默认为一个帧的中间部分,我们将这部分内存顺着帧的开头向后存储
	{
		appendFrameData(rtp, dataPos);
	}
}

void PsStreamServer::resolvePsPacket(RtpSource* rtp, int dataPos)
{
	if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
		rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xba)
	{
		uint8_t expand_size = rtp->data[dataPos + 13] & 0x07;//扩展字节
		dataPos += 14 + expand_size;//ps包头14
									/******  i 帧  ******/
		if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
			rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xbb)//0x000001bb ps system header
		{
			uint16_t psh_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];//psh长度
			dataPos += 6 + psh_size;
			if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
				rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xbc)//0x000001bc ps system map
			{
				uint16_t psm_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];
				dataPos += 6 + psm_size;
			}
			else
			{
				printf("no system map and no video stream\n");
				return;
			}
		}
	}
	resolvePesPacket(rtp, dataPos);
}

void PsStreamServer::resolvePesPacket(RtpSource* rtp, int dataPos)
{
	/******  统一 帧  ******/
	while (true)
	{
		if (rtp->data[dataPos + 0] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
			rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xe0)
		{
			getVideoFrame(rtp, dataPos);
		}
		else if (rtp->data[dataPos] == 0x00 && rtp->data[dataPos + 1] == 0x00 &&
			rtp->data[dataPos + 2] == 0x01 && rtp->data[dataPos + 3] == 0xc0)
		{
			getAudioFrame(rtp, dataPos);
		}
		else
		{
			// printf("no valid stream c0 or e0\n");
			break;
		}
		uint16_t stream_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];
		dataPos += 6 + stream_size;
		if (rtp->len - dataPos < 4)
			break;
	}
}

void PsStreamServer::getVideoFrame(RtpSource* rtp, int dataPos)
{
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;

	rtp->curStream = VIDEO_STREAM;
	uint16_t h264_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];
	uint8_t expand_size = rtp->data[dataPos + 8];
	rtp->frameSize = h264_size - 3 - expand_size;
	dataPos += 9 + expand_size;
	//全部写入并保存帧
	if (rtp->frameSize <= rtp->len - dataPos)
	{
		memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->frameSize);
		rtp->frameLen += rtp->frameSize;
		dataPos += rtp->frameSize;
		writeVideoFrame(rtp);
	}
	else
	{
		memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->len - dataPos);
		rtp->frameLen += rtp->len - dataPos;
	}
}

void PsStreamServer::getAudioFrame(RtpSource* rtp, int dataPos)
{
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;

	rtp->curStream = AUDIO_STREAM;
	uint16_t stream_size = rtp->data[dataPos + 4] << 8 | rtp->data[dataPos + 5];
	uint8_t expand_size = rtp->data[dataPos + 8];
	rtp->frameSize = stream_size - 3 - expand_size;
	dataPos += 9 + expand_size;
	//全部写入并保存帧
	if (rtp->frameSize <= rtp->len - dataPos)
	{
		memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->frameSize);
		rtp->frameLen += rtp->frameSize;
		dataPos += rtp->frameSize;
		writeAudioFrame(rtp);
	}
	else
	{
		memcpy(rtp->frameBuff, rtp->data + dataPos, rtp->len - dataPos);
		rtp->frameLen += rtp->len - dataPos;
	}
}

void PsStreamServer::writeVideoFrame(RtpSource* rtp)
{
	if (rtp->frameSize != rtp->frameLen)
		printf("error:frameSize=%d bufflen=%d\n", rtp->frameSize, rtp->frameLen);
	if (rtp->isFrameStart)
	{
		m_videoDecoder->decodeFrame(rtp->h264Buff, rtp->h264Len);
		rtp->h264Len = 0;
	}
	memcpy(rtp->h264Buff+rtp->h264Len, rtp->frameBuff, rtp->frameLen);
	rtp->h264Len += rtp->frameLen;
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;
}

void PsStreamServer::writeAudioFrame(RtpSource* rtp)
{
	if (rtp->frameLen == 0)
		printf("audio len=0\n");
	if (rtp->frameSize != rtp->frameLen)
		printf("error:frameSize=%d bufflen=%d\n", rtp->frameSize, rtp->frameLen);
	m_audioDecoder->aac_decode_frame(rtp->frameBuff, rtp->frameLen);
	memset(rtp->frameBuff, 0, sizeof(rtp->frameBuff));
	rtp->frameLen = 0;
	rtp->frameSize = 0;
}

void PsStreamServer::appendFrameData(RtpSource* rtp, int dataPos)
{
	if (rtp->len - dataPos + rtp->frameLen >= rtp->frameSize)
	{
		int len = rtp->frameSize - rtp->frameLen;
		memcpy(rtp->frameBuff + rtp->frameLen, rtp->data + dataPos, len);
		rtp->frameLen += len;
		if (VIDEO_STREAM == rtp->curStream)
			writeVideoFrame(rtp);
		else if (AUDIO_STREAM == rtp->curStream)
			writeAudioFrame(rtp);

		if (rtp->len > len)
			resolvePsPacket(rtp, dataPos + len);
	}
	else
	{
		memcpy(rtp->frameBuff + rtp->frameLen, rtp->data + dataPos, rtp->len - dataPos);
		rtp->frameLen += rtp->len - dataPos;
	}
}