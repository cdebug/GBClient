#ifndef COMMON_H
#define COMMON_H
#include <iostream>
#include <vector>
#include <sstream>
#include "tinyxml\tinyxml.h"

enum SocketMethod {
	EstablishForward,
	CancelForward,
};

enum {
	UDP_PROXY,
	TCP_PROXY
};

static std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

#define CHECK_LENGTH 20       //检查是否为utf8编码时所检查的字符长度   

static int is_utf8_string(char *utf)
{
	int length = strlen(utf);
	int check_sub = 0;
	int i = 0;

	if (length > CHECK_LENGTH)  //只取前面特定长度的字符来验证即可  
	{
		length = CHECK_LENGTH;
	}

	for (; i < length; i++)
	{
		if (check_sub == 0)
		{
			if ((utf[i] >> 7) == 0)         //0xxx xxxx  
			{
				continue;
			}
			else if ((utf[i] & 0xE0) == 0xC0) //110x xxxx  
			{
				check_sub = 1;
			}
			else if ((utf[i] & 0xF0) == 0xE0) //1110 xxxx  
			{
				check_sub = 2;
			}
			else if ((utf[i] & 0xF8) == 0xF0) //1111 0xxx  
			{
				check_sub = 3;
			}
			else if ((utf[i] & 0xFC) == 0xF8) //1111 10xx  
			{
				check_sub = 4;
			}
			else if ((utf[i] & 0xFE) == 0xFC) //1111 110x  
			{
				check_sub = 5;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			if ((utf[i] & 0xC0) != 0x80)
			{
				return 0;
			}
			check_sub--;
		}
	}
	return 1;
}

static std::string getTypeContent(std::string buff, const char* type)
{
	std::string ret;
	TiXmlDocument* myDocument = new TiXmlDocument();
	myDocument->Parse(buff.c_str());
	TiXmlElement* rootElement = myDocument->RootElement();
	TiXmlElement* ele;
	for (ele = rootElement->FirstChildElement(); ele; ele = ele->NextSiblingElement())
	{
		if (strcmp(ele->Value(), type) == 0)
		{
			ret = ele->GetText();
			break;
		}
	}
	delete myDocument;
	return ret;
}

static std::string getRootNode(std::string buff)
{
	std::string ret;
	TiXmlDocument* myDocument = new TiXmlDocument();
	myDocument->Parse(buff.c_str());
	TiXmlElement* rootElement = myDocument->RootElement();
	ret = rootElement->Value();
	delete myDocument;
	return ret;
}

static std::list<std::map<std::string, std::string>>
getDeviceChannels(std::string buff)
{
	std::list<std::map<std::string, std::string>> list;
	TiXmlDocument* myDocument = new TiXmlDocument();
	myDocument->Parse(buff.c_str());
	TiXmlElement* rootElement = myDocument->RootElement();
	TiXmlElement* deviceListEle;
	for (deviceListEle = rootElement->FirstChildElement(); deviceListEle; deviceListEle = deviceListEle->NextSiblingElement())
	{
		if (strcmp(deviceListEle->Value(), "DeviceList") == 0)
			break;
	}
	if (deviceListEle)
	{
		for (auto channelEle = deviceListEle->FirstChildElement(); channelEle; channelEle = channelEle->NextSiblingElement())
		{
			if (strcmp(channelEle->Value(), "Item") == 0)
			{
				std::map<std::string, std::string> map;
				for (auto info = channelEle->FirstChildElement(); info; info = info->NextSiblingElement())
				{
					if (info->FirstChildElement())
						continue;
					std::string key, value;
					if (info->Value())
						key = info->Value();
					if (info->GetText())
						value = info->GetText();
					map[key] = value;
				}
				if (map.size() > 0)
					list.push_back(map);
			}
		}
	}
	delete myDocument;

	return list;
}

enum StreamType {
	VIDEO_STREAM,
	AUDIO_STREAM,
};

enum {
	UDP_STREAM,
	TCP_STREAM
};

struct RtpSource
{
	uint8_t data[4096];
	int len;
	uint32_t ssrc;

	uint8_t frameBuff[409600];
	int frameLen;
	int frameSize;
	int curStream;
	uint8_t h264Buff[409600];
	int h264Len = 0;
	bool isFrameStart;
};

#endif //COMMON_H