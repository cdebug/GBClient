#include "gbclient.h"
#include <QSettings>
#include <QMessageBox>
#include "videowidget.h"
#include "common.h"

extern std::string g_localIp;

GBClient::GBClient(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
	connect(ui.listen_btn, &QPushButton::clicked, this, &GBClient::onListen);
	connect(ui.stop_listen_btn, &QPushButton::clicked, this, &GBClient::onStopListen);
	connect(ui.query_btn, &QPushButton::clicked, this, &GBClient::onQuery);
	connect(ui.play_btn, &QPushButton::clicked, this, &GBClient::onPlay);
	connect(ui.stop_btn, &QPushButton::clicked, this, &GBClient::onStop);
	setConfig();
	ui.stop_listen_btn->hide();
	ui.sip_ip_line->setText(QString::fromStdString(g_localIp));
	VideoWidget* video0 = new VideoWidget;
	VideoWidget* video1 = new VideoWidget;
	VideoWidget* video2 = new VideoWidget;
	VideoWidget* video3 = new VideoWidget;
	ui.video_layout->addWidget(video0, 0, 0);
	ui.video_layout->addWidget(video1, 0, 1);
	ui.video_layout->addWidget(video2, 1, 0);
	ui.video_layout->addWidget(video3, 1, 1);
	m_videos << video0 << video1 << video2 << video3;
	connect(video0, &VideoWidget::clicked, this, &GBClient::onVideoClicked);
	connect(video1, &VideoWidget::clicked, this, &GBClient::onVideoClicked);
	connect(video2, &VideoWidget::clicked, this, &GBClient::onVideoClicked);
	connect(video3, &VideoWidget::clicked, this, &GBClient::onVideoClicked);
}

void GBClient::setConfig()
{
	QSettings settings("config.ini", QSettings::IniFormat);
	ui.sip_id_line->setText(settings.value("LocalSip/SipId").toString());
	ui.sip_port_line->setText(settings.value("LocalSip/SipPort").toString());
	ui.username_line->setText(settings.value("LocalSip/Username").toString());
	ui.password_line->setText(settings.value("LocalSip/Password").toString());
	ui.proxy_combo->setCurrentIndex(settings.value("LocalSip/SipProxy").toInt());
}

void GBClient::saveConfig()
{
	QSettings settings("config.ini", QSettings::IniFormat);
	settings.setValue("LocalSip/SipId", ui.sip_id_line->text());
	settings.setValue("LocalSip/SipPort", ui.sip_port_line->text());
	settings.setValue("LocalSip/Username", ui.username_line->text());
	settings.setValue("LocalSip/Password", ui.password_line->text());
	settings.setValue("LocalSip/SipProxy", ui.proxy_combo->currentIndex());
}

void GBClient::onListen()
{
	QString sipId = ui.sip_id_line->text();
	QString username = ui.username_line->text();
	QString password = ui.password_line->text();
	int port = ui.sip_port_line->text().toInt();
	int proxy = ui.proxy_combo->currentIndex();
	if (sipId.length() != 20)
	{
		QMessageBox::warning(this, "Attention", "Please input a valid sip id!");
		return;
	}
	if (port < 0 || port > 65535)
	{
		QMessageBox::warning(this, "Attention", "Please input a valid sip port!");
		return;
	}
	if (username.isEmpty())
	{
		QMessageBox::warning(this, "Attention", "Please input a valid username!");
		return;
	}
	if (password.isEmpty())
	{
		QMessageBox::warning(this, "Attention", "Please input a valid password!");
		return;
	}
	SipConfigInfo info;
	info._sipId = sipId.toStdString();
	info._username = username.toStdString();
	info._password = password.toStdString();
	info._sipListenPort = port;
	info._sipProxy = proxy;
	m_sipContact.setLocalConfig(info);
	m_sipContact.initialize();
	m_sipContact.startServer();
	ui.device_tree->addTopLevelItem(new QTreeWidgetItem(QStringList() << "sip:"+sipId));
	messageOutput(QString("Sip %1 listen on %2(%3)\n").arg(sipId).arg(port).arg(proxy==0?"UDP":"TCP"));
	saveConfig();
	ui.listen_btn->hide();
	ui.stop_listen_btn->show();
	ui.sip_info_area->setEnabled(false);
	ui.stream_combo->clear();
	if (ui.proxy_combo->currentIndex() == 0)
	{
		ui.stream_combo->addItem("UDP");
		ui.stream_combo->addItem("TCP");
	}
	else
	{
		ui.stream_combo->addItem("TCP");
	}
	g_localIp = ui.sip_ip_line->text().toStdString();
}

void GBClient::onStopListen()
{
	ui.listen_btn->show();
	ui.stop_listen_btn->hide();
	ui.sip_info_area->setEnabled(true);
	//m_sipContact.
}

void GBClient::onQuery()
{
	if(ui.device_tree->currentItem()->parent() == ui.device_tree->topLevelItem(0))
		m_sipContact.queryCatalog(ui.device_tree->currentItem()->text(0).toStdString());
}

void GBClient::onPlay()
{
	if (ui.device_tree->currentItem()->parent() && ui.device_tree->currentItem()->parent()->parent() == ui.device_tree->topLevelItem(0))
	{
		QString channelId = ui.device_tree->currentItem()->data(0, Qt::UserRole).toString();
		int proxy;
		if (ui.stream_combo->currentText().contains("TCP"))
			proxy = TCP_STREAM;
		else
			proxy = UDP_STREAM;
		VideoWidget* video = nullptr;
		if (m_selectedVideo)
		{
			video = m_selectedVideo;
			if (video->isRunning())
				video->stop();
		}
		else
		{
			for (int i = 0; i < m_videos.size(); ++i)
			{
				if (!m_videos.at(i)->isRunning())
				{
					video = m_videos.at(i);
					break;
				}
			}
		}
		if (video)
		{
			int port = video->allocReceivePort(channelId, proxy);
			if (port > 0)
			{
				m_sipContact.handleInvite(channelId.toStdString(), port, proxy);
			}
		}
	}
}

void GBClient::onStop()
{
	if (m_selectedVideo)
		m_selectedVideo->stop();
}

void GBClient::deviceAppended(QString devId)
{
	ui.device_tree->topLevelItem(0)->addChild(new QTreeWidgetItem(QStringList() << devId));
	ui.device_tree->expandAll();
}

void GBClient::deviceRemoved(QString devId)
{
	QTreeWidgetItem* parent = ui.device_tree->topLevelItem(0);
	for (int i = 0; i < parent->childCount(); ++i)
	{
		if (parent->child(i)->text(0) == devId)
			delete parent->child(i);
	}
}

void GBClient::messageOutput(QString str)
{
	if (str.endsWith('\n'))
	{
		int pos = str.lastIndexOf('\n');
		str.remove(pos, 1);
	}
	ui.message_browser->append(str);
}

void GBClient::channelReceived(QString devId, QMap<std::string, std::string> channel)
{
	printf("channelReceived\n");
	for (int i = 0; i < ui.device_tree->topLevelItem(0)->childCount(); ++i)
	{
		QTreeWidgetItem* devItem = ui.device_tree->topLevelItem(0)->child(i);
		if (devItem->text(0) == devId)
		{
			std::string channelId = channel["DeviceID"];
			bool matchFlag = false;
			for (int j = 0; j < devItem->childCount(); ++j)
			{
				if (devItem->child(j)->data(0, Qt::UserRole).toString().toStdString() == channelId)
				{
					matchFlag = true;
					break;
				}
			}
			if (!matchFlag)
			{
				QTreeWidgetItem* chlItem = new QTreeWidgetItem;
				chlItem->setText(0, QString::fromStdString(channel["Name"]));
				chlItem->setData(0, Qt::UserRole, QString::fromStdString(channelId));
				devItem->addChild(chlItem);
			}
			return;
		}
	}
}

void GBClient::onVideoClicked()
{
	VideoWidget* video = qobject_cast<VideoWidget*>(sender());
	if (!video)
		return;
	if (m_selectedVideo)
	{
		m_selectedVideo->setProperty("selected", false);
		m_selectedVideo->setStyleSheet(m_selectedVideo->styleSheet());
		if (m_selectedVideo == video)
		{
			m_selectedVideo = nullptr;
			return;
		}
	}
	m_selectedVideo = video;
	m_selectedVideo->setProperty("selected", true);
	m_selectedVideo->setStyleSheet(m_selectedVideo->styleSheet());
}