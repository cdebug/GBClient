#include "deviceinfos.h"
#include <stdio.h>
#include <thread>

#define DEVICE_CHECK_INTERVAL 10
DeviceInfos* DeviceInfos::s_instance = nullptr;

DeviceInfos::DeviceInfos()
{
}

DeviceInfos::~DeviceInfos()
{
}

DeviceInfos* DeviceInfos::instance()
{
	if (!s_instance)
		s_instance = new DeviceInfos;
	return s_instance;
}

void DeviceInfos::initialize()
{

}

void DeviceInfos::startServer()
{
	std::thread(&DeviceInfos::deviceChecking, this).detach();
}

void DeviceInfos::deviceChecking()
{
	while (1)
	{
		std::this_thread::sleep_for(std::chrono::seconds(DEVICE_CHECK_INTERVAL));
		std::lock_guard<std::mutex> lk(_iterMutex);
		for (auto it = m_devs.begin(); it != m_devs.end();)
		{
			if ((*it)->getExpires() > DEVICE_CHECK_INTERVAL)
			{
				(*it)->setExpires((*it)->getExpires() - DEVICE_CHECK_INTERVAL);
				++it;
			}
			else
			{
				messageOutput(QString("Device %1 heart beat timeout, removed\n").arg(QString::fromStdString((*it)->getDeviceId())));
				deviceRemoved(QString::fromStdString((*it)->getDeviceId()));
				it = m_devs.erase(it);
			}
		}
	}
}

void DeviceInfos::removeItem(std::string devId)
{
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto it = m_devs.begin(); it != m_devs.end();)
	{
		if (devId == (*it)->getDeviceId())
		{
			delete (*it);
			m_devs.erase(it);
			messageOutput(QString("Device removed %s\n").arg(QString::fromStdString(devId)));
			deviceRemoved(QString::fromStdString(devId));
			return;
		}
		else
		{
			++it;
		}
	}
	printf("No device removed %s\n", devId.c_str());
}

bool DeviceInfos::deviceExists(std::string devId)
{
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->getDeviceId() == devId)
		{
			return true;
		}
	}
	return false;
}
void DeviceInfos::appendDevice(std::string devId, std::string addr, int port, int maxExpires, int expires)
{
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->getDeviceId() == devId)
		{
			//Refresh device's data while exists
			device->setIp(addr);
			device->setPort(port);
			device->setMaxPires(maxExpires);
			device->setExpires(expires);
			messageOutput(QString("Device %1 refreshed! %2 %3 %4\n").arg(QString::fromStdString(devId)).arg(QString::fromStdString(addr)).arg(port).arg(expires));
			return;
		}
	}
	//Add new device while not exists
	DeviceItem* item = new DeviceItem;
	item->setDeviceId(devId);
	item->setIp(addr);
	item->setPort(port);
	item->setMaxPires(maxExpires);
	item->setExpires(expires);
	m_devs.push_back(item);
	messageOutput(QString("Device %1 added! %2 %3 %4\n").arg(QString::fromStdString(devId)).arg(QString::fromStdString(addr)).arg(port).arg(expires));
	deviceAppended(QString::fromStdString(devId));
}

std::string DeviceInfos::getDeviceByChannelNp(std::string channelNo)
{
	printf("Get device by channelNo %s\n", channelNo.c_str());
	std::string devId;
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->_channels.find(channelNo) != device->_channels.end())
		{
			devId = device->getDeviceId();
			printf("One device matched channelNo has been found\n");
			return devId;
		}
	}
	printf("No match device to channelNo %s\n", channelNo.c_str());
	return devId;
}

bool DeviceInfos::keepAlive(std::string devId)
{
	printf("Device keep alive %s\n", devId.c_str());
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto item : m_devs)
	{
		if (devId == item->getDeviceId())
		{
			item->setExpires(item->getMaxExpires());
			messageOutput(QString::fromStdString("Device keep alive! %1\n").arg(QString::fromStdString(devId)));
			return true;
		}
	}
	printf("No match device %s for keep alive!\n", devId.c_str());
	return false;
}

bool DeviceInfos::appendDeviceChannels(std::string devId, std::list<std::map<std::string, std::string>> channelList)
{
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		//ШЅжи
		if (device->getDeviceId() == devId)
		{
			for (auto elem : channelList)
			{
				device->addChannels(elem["DeviceID"], elem);
				channelReceived(QString::fromStdString(devId), QMap<std::string, std::string>(elem));
			}
			return true;
		}
	}
	return false;
}

bool DeviceInfos::getDeviceInfo(std::string devId, std::string& ip, int& port)
{
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->getDeviceId() == devId)
		{
			ip = device->getIp();
			port = device->getPort();
			printf("Got device info: %s %s %d\n", devId.c_str(), ip.c_str(), port);
			return true;
		}
	}
	printf("Cannot get device info, because of no device %s\n", devId.c_str());
	return false;
}

bool DeviceInfos::getDeviceInfoByChannelId(std::string channelId, std::string& devId, std::string& addr, int& port)
{
	printf("Get device info by channelNo %s\n", channelId.c_str());
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->_channels.find(channelId) != device->_channels.end())
		{
			devId = device->getDeviceId();
			addr = device->getIp();
			port = device->getPort();
			return true;
		}
	}
	printf("No match device info to channelNo %s\n", channelId.c_str());
	return false;
}

std::list<std::string> DeviceInfos::getDeviceIds()
{
	std::list<std::string> list;
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		list.push_back(device->getDeviceId());
	}
	printf("Got device ids,count: %d\n", list.size());
	return list;
}

std::shared_ptr<ChannelItem> DeviceInfos::getChannel(std::string channelId)
{
	std::shared_ptr<ChannelItem> ptr;
	std::lock_guard<std::mutex> lk(_iterMutex);
	for (auto device : m_devs)
	{
		if (device->_channels.find(channelId) != device->_channels.end())
		{
			ptr = device->_channels[channelId];
			break;
		}
	}
	return ptr;
}