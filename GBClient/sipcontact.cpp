#include "sipcontact.h"
#include <osip2/osip_mt.h>
#include <osip2/osip.h>
#include <osipparser2/osip_message.h>
#include <osipparser2/osip_parser.h>
#include <osipparser2/osip_md5.h>
#include <eXosip2/eX_call.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include "common.h"
#include <list>
#include <thread>
int cid;
extern std::string g_localIp;

SipContact::SipContact(void)
{
}

SipContact::~SipContact(void)
{
}

void SipContact::setLocalConfig(SipConfigInfo sipConfig)
{
	m_localConfig = sipConfig;
}

int SipContact::initialize()
{
	int i;
	TRACE_INITIALIZE(6, NULL);
	m_ctx = eXosip_malloc();
	if (m_ctx == NULL)
	{
		printf("eXosip_malloc failed\n");
		return -1;
	}
	i = eXosip_init(m_ctx);
	if (i != 0)
	{
		printf("eXosip_init failed\n");
		return -1;
	}
	int transp;
	if (m_localConfig._sipProxy == 1)
		transp = IPPROTO_TCP;
	else
		transp = IPPROTO_UDP;
	i = eXosip_listen_addr(m_ctx, transp, NULL, m_localConfig._sipListenPort, AF_INET, 0);
	if (i != 0)
	{
		eXosip_quit(m_ctx);
		printf("could not initialize transport layer\n");
		return -1;
	}
	printf("Protrol %s listen on %d\n", m_localConfig._sipProxy == 1 ? "TCP" : "UDP", m_localConfig._sipListenPort);
	eXosip_clear_authentication_info(m_ctx);
	eXosip_add_authentication_info(m_ctx, m_localConfig._sipId.c_str(), m_localConfig._sipId.c_str(), "12345678", "MD5", NULL);//添加认证信息
	return 0;
}

void SipContact::startServer()
{
	std::thread(&SipContact::enterSipEventLoop, this).detach();
}

void SipContact::enterSipEventLoop()
{
	eXosip_event_t *je;
	for (;;)
	{
		je = eXosip_event_wait(m_ctx, 0, 50);
		eXosip_lock(m_ctx);
		eXosip_default_action(m_ctx, je);
		eXosip_unlock(m_ctx);
		if (je == NULL)
			continue;
		switch (je->type)
		{
		case EXOSIP_MESSAGE_NEW://新的消息到来   
			if (MSG_IS_MESSAGE(je->request))//如果接受到的消息类型是MESSAGE         
			{
				printf("MSG_IS_MESSAGE!\n");
				osip_body_t *body;
				osip_message_get_body(je->request, 0, &body);

				std::string cmdtype = getTypeContent(body->body, "CmdType");
				std::string rootNode = getRootNode(body->body);
				if (cmdtype == "Catalog" && rootNode == "Response")
				{
					if (parserCatalog(je))
						answer200OK(je);
				}
				else if (cmdtype == "Keepalive")
				{
					std::string devId = getTypeContent(body->body, "DeviceID");
					//如果设备存在则返回true
					if (DeviceInfos::instance()->keepAlive(devId))
						answer200OK(je);
				}
				else
				{
					//按照规则，需要回复200 OK信息     
					printf("root:%s cmdType:%s\n", rootNode.c_str(), cmdtype.c_str());
					// answer200OK(je);
				}
			}
			else if (MSG_IS_REGISTER(je->request))
			{
				printf("MSG_IS_REGISTER!\n");
				osip_header_t* expires;
				osip_message_get_expires(je->request, 0, &expires);
				if (expires && expires->hvalue)
				{
					if (atoi(expires->hvalue) > 0)
						handleRegister(je);
					else
						handleLogout(je);
				}
			}
			else
			{
				printf("EXOSIP_MESSAGE_NEW!\n");
				//按照规则，需要回复200 OK信息
				answer200OK(je);
			}
			break;

		case EXOSIP_REGISTRATION_SUCCESS:// 注册成功
		{
			printf("EXOSIP_REGISTRATION_SUCCESS:%d\n", je->rid);
			break;
		}
		case EXOSIP_REGISTRATION_FAILURE:// 注册失败
			printf("EXOSIP_REGISTRATION_FAILURE\n");
			if (je->response != NULL &&
				(je->response->status_code == 401 || je->response->status_code == 407))
			{
			}
			else
			{
				printf("register really failed\n");
			}
			break;

		case EXOSIP_CALL_INVITE:
			break;

		case EXOSIP_CALL_PROCEEDING:
			printf("proceeding!\n");
			break;

		case EXOSIP_CALL_RINGING:
			printf("ringing!\n");
			break;
		case EXOSIP_CALL_NOANSWER:
			printf("no answer!\n");
			break;

		case EXOSIP_CALL_CLOSED:
			printf("the other sid closed!\n");
			break;

		case EXOSIP_CALL_ACK:
			printf("ACK received!\n");
			break;

		case EXOSIP_CALL_REQUESTFAILURE:
			printf("%s\n", je->textinfo);
			break;

		case EXOSIP_CALL_RELEASED:
			printf("%s\n", je->textinfo);
			break;
		case EXOSIP_IN_SUBSCRIPTION_NEW:
			printf("EXOSIP_IN_SUBSCRIPTION_NEW\n");
			break;
		case EXOSIP_CALL_ANSWERED:
		{
			printf("EXOSIP_CALL_ANSWERED\n");
			answerACK(je);
		}
		case EXOSIP_MESSAGE_ANSWERED:
		{
			printf("EXOSIP_CALL_ANSWERED || EXOSIP_MESSAGE_ANSWERED\n");
			osip_message_t* ack;
			int i = eXosip_call_build_ack(m_ctx, je->did, &ack);
			if (i != 0)
				break;;
			eXosip_call_send_ack(m_ctx, je->did, ack);
			break;
		}
		default:
			printf("other response!\n");
			break;
		}
		eXosip_event_free(je);
	}
}

bool SipContact::parserCatalog(eXosip_event_t* je)
{
	printf("Catalog response reviced !\n");
	std::string xml_body;
	osip_body_t* body;
	osip_message_get_body(je->request, 0, &body);
	xml_body = body->body;
	std::string devId = getTypeContent(xml_body, "DeviceID");
	std::list<std::map<std::string, std::string>> list = getDeviceChannels(xml_body);
	if (DeviceInfos::instance()->appendDeviceChannels(devId, list))
	{
		printf("Catalog Response parsered %s!\n", devId.c_str());
		return true;
	}
	else
	{
		printf("No match device %s for parser catalog\n", devId.c_str());
		return false;
	}
}

void SipContact::handleRegister(eXosip_event_t* je)
{
	std::string localSipId = m_localConfig._sipId;
	osip_from_t* from = osip_message_get_from(je->request);
	std::string devId;
	if (from && from->url && from->url->username)
		devId = from->url->username;
	if (devId.size() < 20 /*|| devId.substr(0,4) != localSipId.substr(0,4)*/)
	{
		printf("Handle register invilid device id %s\n", devId.c_str());
		return;
	}
	osip_message_t* asw_register = NULL;
	char WWW_Authenticate[512] = { 0 };
	osip_authorization_t *AuthHeader;
	osip_message_get_authorization(je->request, 0, &AuthHeader);
	if (!AuthHeader) //Question
	{
		sprintf_s(WWW_Authenticate, "Digest realm=\"10.0.0.4\",algorithm=MD5,nonce=\"%d\"", osip_build_random_number());
		eXosip_lock(m_ctx);
		int i = eXosip_message_build_answer(m_ctx, je->tid, 401, &asw_register);
		eXosip_unlock(m_ctx);
		if (i != 0)
			return;
		osip_message_set_header(asw_register, "WWW-Authenticate", WWW_Authenticate);
		eXosip_lock(m_ctx);
		eXosip_message_send_answer(m_ctx, je->tid, 401, asw_register);
		eXosip_unlock(m_ctx);
	}
	else //Test and Verify
	{
		int i = eXosip_message_build_answer(m_ctx, je->tid, 200, &asw_register);
		if (i != 0)
			return;
		osip_header_t* expires;
		osip_message_get_expires(je->request, 0, &expires);

		char addr[20];
		int port;
		memset(addr, '\0', strlen(addr));
		osip_via_t* via = nullptr;
		osip_message_get_via(asw_register, 0, &via);
		if (!via || !via->host)
		{
			eXosip_lock(m_ctx);
			eXosip_message_send_answer(m_ctx, je->tid, 400, NULL);
			eXosip_unlock(m_ctx);
			printf("Register answer no via\n");
			return;
		}
		osip_generic_param_t* br = nullptr;
		osip_via_param_get_byname(via, "received", &br);
		if (br != NULL && br->gvalue != NULL)
			strcpy_s(addr, br->gvalue);
		else
			strcpy_s(addr, via->host);

		osip_via_param_get_byname(via, "rport", &br);
		if (!br || !br->gvalue)
		{
			eXosip_message_send_answer(m_ctx, je->tid, 400, NULL);
			printf("Register answer no rport\n");
			return;
		}
		port = atoi(br->gvalue);
		printf("Ip:%s port:%d\n", addr, port);
		eXosip_lock(m_ctx);
		eXosip_message_send_answer(m_ctx, je->tid, 200, asw_register);
		eXosip_unlock(m_ctx);
		DeviceInfos::instance()->appendDevice(devId, addr, port, atoi(expires->hvalue), atoi(expires->hvalue));
	}
}

void SipContact::handleLogout(eXosip_event_t* je)
{
	osip_message_t* asw_register = NULL;
	//Define authentication variables
	char WWW_Authenticate[512] = { 0 };
	osip_authorization_t *AuthHeader;
	osip_message_get_authorization(je->request, 0, &AuthHeader);

	if (!AuthHeader) //Question
	{
		sprintf_s(WWW_Authenticate, "Digest realm=\"10.0.0.4\",algorithm=MD5,nonce=\"%d\"", osip_build_random_number());
		eXosip_lock(m_ctx);
		int i = eXosip_message_build_answer(m_ctx, je->tid, 401, &asw_register);
		eXosip_unlock(m_ctx);
		if (i != 0)
			return;
		osip_message_set_header(asw_register, "WWW-Authenticate", WWW_Authenticate);
		eXosip_lock(m_ctx);
		eXosip_message_send_answer(m_ctx, je->tid, 401, asw_register);
		eXosip_unlock(m_ctx);
	}
	else //Test and Verify
	{
		printf("Logout with authorization!\n");
		eXosip_lock(m_ctx);
		int i = eXosip_message_build_answer(m_ctx, je->tid, 200, &asw_register);
		eXosip_unlock(m_ctx);
		if (i != 0)
			return;
		eXosip_lock(m_ctx);
		eXosip_message_send_answer(m_ctx, je->tid, 200, asw_register);
		eXosip_unlock(m_ctx);

		osip_from_t* from = osip_message_get_from(je->request);
		if (from && from->url && from->url->username)
		{
			std::string devId = from->url->username;
			DeviceInfos::instance()->removeItem(devId);
		}
	}
}

void SipContact::queryCatalog(std::string devId)
{
	std::string addr;
	int port;
	if (!DeviceInfos::instance()->getDeviceInfo(devId, addr, port))
		return;
	printf("Query device catalog\n");
	int localPort = m_localConfig._sipListenPort;
	char to[100];/*sip:主叫用户名@被叫IP地址*/
	char from[100];/*sip:被叫IP地址:被叫IP端口*/
	char xml_body[4096];
	memset(to, 0, 100);
	memset(from, 0, 100);
	snprintf(to, sizeof(to), "sip:%s@%s:%d", devId.c_str(), addr.c_str(), port);
	snprintf(from, sizeof(from), "sip:%s@%s:%d", m_localConfig._sipId.c_str(), m_localConfig._sipId.substr(0, 10).c_str(), localPort);
	osip_message_t* req;
	int i = eXosip_message_build_request(m_ctx, &req, "MESSAGE", to, from, NULL);/*构建"MESSAGE"请求*/
	if (i != 0)
		return;
	snprintf(xml_body, 4096, "<?xml version=\"1.0\"?>\r\n"
		"<Query>\r\n"
		"<CmdType>Catalog</CmdType>\r\n"
		"<SN>1</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"</Query>\r\n", devId.c_str());
	osip_message_set_body(req, xml_body, strlen(xml_body));
	osip_message_set_content_type(req, "Application/MANSCDP+xml");
	eXosip_lock(m_ctx);
	eXosip_message_send_request(m_ctx, req);/*回复"MESSAGE"请求*/
	eXosip_unlock(m_ctx);
	printf("Query device catalog sent %s\n", devId.c_str());
}

void SipContact::answer200OK(eXosip_event_t* je)
{
	eXosip_lock(m_ctx);
	osip_message_t *answer = NULL;
	int i = eXosip_message_build_answer(m_ctx, je->tid, 200, &answer);
	eXosip_unlock(m_ctx);
	if (i != 0)
		return;
	eXosip_lock(m_ctx);
	eXosip_message_send_answer(m_ctx, je->tid, 200, answer);
	eXosip_unlock(m_ctx);
}

void SipContact::testInvite()
{
	std::string localSipId = m_localConfig._sipId;
	int localSipPort = m_localConfig._sipListenPort;
	std::string channelNo = "32010000001310000121";
	std::string devId = "32010000001310000001";
	std::string addr;
	int port;
	if (!DeviceInfos::instance()->getDeviceInfo(devId, addr, port))
		return;
	char buff[4096];
	sprintf_s(buff, "v=0\r\n"
		"o=%s 0 0 IN IP4 192.168.31.13\r\n"
		"s=Play\r\n"
		"c=IN IP4 192.168.31.13\r\n"
		"t=0 0\r\n"
		"m=video 10000 RTP/AVP 96 98 97\r\n"
		"a=recvonly\r\n"
		"a=rtpmap:96 PS/90000\r\n"
		"a=rtpmap:98 H264/90000\r\n"
		"a=rtpmap:97 MPEG4/90000\r\n"
		"a=setup:passive\r\n"
		"a=connection:new\r\n"
		"y=00000121\r\n", channelNo.c_str());
	char to[100];/*sip:主叫用户名@被叫IP地址*/
	char from[100];/*sip:被叫IP地址:被叫IP端口*/
	memset(to, 0, 100);
	memset(from, 0, 100);
	snprintf(to, sizeof(to), "sip:%s@%s:%d", channelNo.c_str(), addr.c_str(), port);
	snprintf(from, sizeof(from), "sip:%s@%s:%d", channelNo.c_str(), localSipId.substr(0, 10).c_str(), localSipPort);
	osip_message_t* invite;
	int i = eXosip_call_build_initial_invite(m_ctx, &invite, to, from, nullptr, nullptr);
	if (i != 0)
		return;
	osip_message_set_supported(invite, "100rel");
	osip_message_set_content_type(invite, "APPLICATION/SDP");
	char subject[4096];
	sprintf_s(subject, "%s:0,%s:0", channelNo.c_str(), "1001");
	osip_message_set_subject(invite, subject);
	osip_message_set_body(invite, buff, strlen(buff));
	eXosip_lock(m_ctx);
	cid = eXosip_call_send_initial_invite(m_ctx, invite);
	eXosip_unlock(m_ctx);
	printf("Invite to device sent!\n");
}

 bool SipContact::handleInvite(std::string channelId, int receivePort, int proxy)
 {
     std::string localSipId = m_localConfig._sipId;
     int localSipPort = m_localConfig._sipListenPort;
     int i;
     char buff[4096];

	 std::string strProxy = "RTP/AVP";
	 if (proxy == TCP_STREAM)
		 strProxy = "TCP/RTP/AVP";
     sprintf(buff, 
		 "v=0\r\n"
		 "o=%s 0 0 IN IP4 %s\r\n"
		 "s=Play\r\n"
		 "c=IN IP4 %s\r\n"
		 "t=0 0\r\n"
		 "m=video %d %s 96 98 97\r\n"
		 "a=recvonly\r\n"
		 "a=rtpmap:96 PS/90000\r\n"
		 "a=rtpmap:98 H264/90000\r\n"
		 "a=rtpmap:97 MPEG4/90000\r\n"
		 "a=setup:passive\r\n"
		 "a=connection:new\r\n"
		 "y=999999\r\n"
		 , channelId.c_str(), g_localIp.c_str(), g_localIp.c_str(), receivePort, strProxy.c_str());
     std::string devId;
     std::string ip;
     int port;
     if(!DeviceInfos::instance()->getDeviceInfoByChannelId(channelId, devId, ip, port))
         return false;
     char to[100];/*sip:主叫用户名@被叫IP地址*/
 	char from[100];/*sip:被叫IP地址:被叫IP端口*/
 	memset(to, 0, 100);
 	memset(from, 0, 100);
 	snprintf(to,sizeof(to), "sip:%s@%s:%d", channelId.c_str(),ip.c_str(), port);
 	snprintf(from,sizeof(from), "sip:%s@%s:%d", channelId.c_str(), localSipId.substr(0,10).c_str(),localSipPort);
     osip_message_t* invite;
     i = eXosip_call_build_initial_invite(m_ctx, &invite, to, from, nullptr, nullptr);
     if(i != 0)
         return false;
     osip_message_set_supported (invite, "100rel");
     osip_message_set_content_type(invite, "APPLICATION/SDP");
     char subject[4096];
     sprintf(subject, "%s:0,%s:0", channelId.c_str(), "1001");
     osip_message_set_subject(invite, subject);
     osip_message_set_body(invite, buff, strlen(buff));
     eXosip_lock(m_ctx);
     eXosip_call_send_initial_invite(m_ctx, invite);
     eXosip_unlock(m_ctx);
     printf("Invite to device sent!\n");
     return true;
 }

void SipContact::answerACK(eXosip_event_t* je)
{
	osip_message_t* ack;
	int i = eXosip_call_build_ack(m_ctx, je->did, &ack);
	if (i != 0)
		return;
	eXosip_lock(m_ctx);
	eXosip_call_send_ack(m_ctx, je->did, ack);
	eXosip_unlock(m_ctx);
}


void SipContact::handleBye(std::string channelId)
{
	/*printf("handleBye %s\n", channelId.c_str());
	int cid, did;
	eXosip_lock(m_ctx);
	eXosip_call_terminate(m_ctx, cid, did);
	eXosip_unlock(m_ctx);
	printf("Bye sent %s\n", channelId.c_str());*/
}

void SipContact::handleForwardBye(std::string channelId)
{
	/*printf("handleBye %s\n", channelId.c_str());
	int cid, did;
	if (cid == 0 || did == 0)
		return;
	eXosip_lock(m_ctx);
	eXosip_call_terminate(m_ctx, cid, did);
	eXosip_unlock(m_ctx);
	printf("Bye sent %s\n", channelId.c_str());
	cancelStreamForward(channelId);*/
}

void SipContact::cancelStreamForward(std::string channelId)
{
}

void SipContact::stop()
{
	eXosip_quit(m_ctx);
}