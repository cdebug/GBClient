#include "gbclient.h"
#include "deviceinfos.h"
#include <QtWidgets/QApplication>
#include "common.h"
QT_BEGIN_NAMESPACE
template <>
struct QMetaTypeId< QMap<std::string, std::string> >
{
	enum { Defined = 1 };
	static int qt_metatype_id()
	{
		static QBasicAtomicInt metatype_id = Q_BASIC_ATOMIC_INITIALIZER(0);
		if (!metatype_id)
			metatype_id = qRegisterMetaType< QMap<std::string, std::string> >("QMap<std::string, std::string>",
				reinterpret_cast< QMap<std::string, std::string> *>(quintptr(-1)));
		return metatype_id;
	}
};
QT_END_NAMESPACE

std::string g_localIp;

bool GetLocalIP(char* ip)
{
	//1.初始化wsa
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0)
	{
		return false;
	}
	//2.获取主机名
	char hostname[256];
	ret = gethostname(hostname, sizeof(hostname));
	if (ret == SOCKET_ERROR)
	{
		return false;
	}
	//3.获取主机ip
	HOSTENT* host = gethostbyname(hostname);
	if (host == NULL)
	{
		return false;
	}
	//4.转化为char*并拷贝返回
	strcpy(ip, inet_ntoa(*(in_addr*)*host->h_addr_list));
	return true;
}

int main(int argc, char *argv[])
{
	char ip[20];
	memset(ip, 0, sizeof(ip));
	GetLocalIP(ip);
	g_localIp = ip;
	printf("local ip: %s\n", g_localIp.c_str());

	qRegisterMetaType<QMap<std::string, std::string>>();
	DeviceInfos::instance()->startServer();
    QApplication a(argc, argv);
    GBClient w;
    w.show();
	QObject::connect(DeviceInfos::instance(), &DeviceInfos::deviceAppended, &w, &GBClient::deviceAppended);
	QObject::connect(DeviceInfos::instance(), &DeviceInfos::deviceRemoved, &w, &GBClient::deviceRemoved);
	QObject::connect(DeviceInfos::instance(), &DeviceInfos::channelReceived, &w, &GBClient::channelReceived);
	QObject::connect(DeviceInfos::instance(), &DeviceInfos::messageOutput, &w, &GBClient::messageOutput);
    return a.exec();
}
