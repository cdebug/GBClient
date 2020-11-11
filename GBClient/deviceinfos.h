#ifndef DEVICEINFOS_H
#define DEVICEINFOS_H
#include <list>
#include <map>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <map>
#include <memory>
#include <mutex>
#include <Windows.h>
#include <QObject>
#include <QMap>
class UpperSipContact;

struct ChannelItem {
	std::map<std::string, std::string> _properties;
	int _tid;
	int _cid;
	int _did;
	bool _streamForward;
	int _streamProxy;
	// std::weak_ptr<UpperSipContact> _caller;
};

struct DeviceItem {
	std::string _deviceId;
	std::string _ip;
	int _port;
	int _expires;
	int _maxExpires;
	std::map<std::string, std::shared_ptr<ChannelItem>> _channels;

	DeviceItem() {}
	~DeviceItem() {}
	std::string getDeviceId() { return _deviceId; }
	void setDeviceId(std::string devId) { _deviceId = devId; }
	std::string getIp() { return _ip; }
	void setIp(std::string ip) { _ip = ip; }
	int getPort() { return _port; }
	void setPort(int port) { _port = port; }
	int getExpires() { return _expires; }
	void setExpires(int expires) { _expires = expires; }
	int getMaxExpires() { return _maxExpires; }
	void setMaxPires(int expires) { _maxExpires = expires; }
	void addChannels(std::string channelId, std::map<std::string, std::string> map)
	{
		for (auto channel : _channels)
		{
			if (channel.second->_properties["DeviceID"] == channelId)
			{
				channel.second->_properties = map;
				return;
			}
		}
		std::shared_ptr<ChannelItem> channel(new ChannelItem);
		channel->_properties = map;
		_channels[channelId] = channel;
	}
};

class DeviceInfos : public QObject
{
	Q_OBJECT
public:
	DeviceInfos();
	~DeviceInfos();
	static DeviceInfos* instance();
	void initialize();
	void startServer();
	void removeItem(std::string);
	bool deviceExists(std::string);
	void appendDevice(std::string, std::string, int, int, int);
	std::string getDeviceByChannelNp(std::string);
	bool keepAlive(std::string);
	bool appendDeviceChannels(std::string, std::list<std::map<std::string, std::string>>);
	bool getDeviceInfo(std::string, std::string&, int&);
	bool getDeviceInfoByChannelId(std::string, std::string&, std::string&, int&);
	std::list<std::string> getDeviceIds();
	std::shared_ptr<ChannelItem> getChannel(std::string);

private:
	void deviceChecking();

	static DeviceInfos* s_instance;
	std::list<DeviceItem*> m_devs;
	std::list<std::string> m_channelKeys;
	std::mutex _iterMutex;
signals:
	void messageOutput(QString);
	void deviceAppended(QString);
	void deviceRemoved(QString);
	void channelReceived(QString, QMap<std::string, std::string>);
};

#endif //DEVICEINFOS_H