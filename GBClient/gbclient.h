#pragma once

#include <QtWidgets/QWidget>
#include "ui_gbclient.h"
#include "sipcontact.h"
class VideoWidget;

class GBClient : public QWidget
{
    Q_OBJECT

public:
    GBClient(QWidget *parent = Q_NULLPTR);

private:
    Ui::GBClientClass ui;
	SipContact m_sipContact;
	QList<VideoWidget*> m_videos;
	VideoWidget* m_selectedVideo = nullptr;

	void setConfig();
	void saveConfig();
private slots:
	void onListen();
	void onStopListen();
	void onQuery();
	void onPlay();
	void onStop();
	void onVideoClicked();

public slots:
	void deviceAppended(QString);
	void deviceRemoved(QString);
	void messageOutput(QString);
	void channelReceived(QString, QMap<std::string, std::string>);
};
