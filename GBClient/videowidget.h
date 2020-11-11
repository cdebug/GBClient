#pragma once

#include <QWidget>
#include "ui_videowidget.h"
class PsStreamServer;
class QTimer;

class VideoWidget : public QWidget
{
	Q_OBJECT

public:
	VideoWidget(QWidget *parent = Q_NULLPTR);
	~VideoWidget();
	int allocReceivePort(QString, int);
	bool isRunning();
	void mousePressEvent(QMouseEvent*);
	void mouseDoubleClickEvent(QMouseEvent*);
	void stop();

protected:
	void paintEvent(QPaintEvent*);

private:
	Ui::VideoWidget ui;
	QString m_channelId;
	PsStreamServer* m_streamServer;
	QTimer* m_mouseClickTimer;
	bool m_bFullScreen = false;
	QPoint m_pos;
	QSize m_size;
	QPixmap m_pixmap;

private slots:
	void setVideoFrame(QImage);
	void setAudioFrame(QByteArray);
	void signalClick();
	void setFullScreen();

signals:
	void clicked();
};
