#ifndef _SIPCLIETN_H_
#define _SIPCLIETN_H_

#include <eXosip2/eXosip.h>
#include <list>
#include <string.h>
#include <iostream>
#include "DeviceInfos.h"

struct DeviceItem;

struct SipConfigInfo
{
	std::string _sipId;
	int _sipListenPort;
	std::string _sipIp;
	int _sipProxy;
	std::string _username;
	std::string _password;
};

class SipContact
{
public:
	SipContact(void);
	~SipContact(void);
	void setLocalConfig(SipConfigInfo);
	int initialize();
	void startServer();
	void enterSipEventLoop();
	bool handleInvite(std::string, int, int);
	void handleBye(std::string channelId);
	void handleForwardBye(std::string channelId);
	void queryCatalog(std::string);
	void stop();
private:
	bool parserCatalog(eXosip_event_t*);
	void handleRegister(eXosip_event_t*);
	void handleLogout(eXosip_event_t*);
	void answer200OK(eXosip_event_t*);
	void answerACK(eXosip_event_t* je);
	void testInvite();
	void cancelStreamForward(std::string);

	eXosip_t* m_ctx;
	SipConfigInfo m_localConfig;
};

#endif