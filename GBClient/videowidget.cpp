#include "videowidget.h"
#include "common.h"
#include "psstreamserver.h"
#include "psstreamtcpserver.h"
#include "psstreamudpserver.h"
#include <thread>
#include <Qtimer>
#include <QPainter>

VideoWidget::VideoWidget(QWidget *parent)
	: QWidget(parent), m_streamServer(nullptr), m_mouseClickTimer(new QTimer(this))
{
	ui.setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
	m_mouseClickTimer->setInterval(1000);
	m_mouseClickTimer->setSingleShot(true);
	connect(m_mouseClickTimer, &QTimer::timeout, this, &VideoWidget::signalClick);
}

VideoWidget::~VideoWidget()
{
}

int VideoWidget::allocReceivePort(QString channelId, int proxy)
{
	if (m_streamServer)
	{
		m_streamServer->disconnect();
		delete m_streamServer;
	}
	m_channelId = channelId;
	if (proxy == TCP_STREAM)
		m_streamServer = new PsStreamTcpServer;
	else
		m_streamServer = new PsStreamUdpServer;
	int port = m_streamServer->allocReceivePort();
	if (port > 0)
	{
		std::thread(&PsStreamServer::executeProcess, m_streamServer).detach();
		connect(m_streamServer->videoDecoder(), &H264Decoder::frameChanged, this, &VideoWidget::setVideoFrame);
		connect(m_streamServer->audioDecoder(), &AACDecoder::frameChanged, this, &VideoWidget::setAudioFrame);
	}
	return port;
}

bool VideoWidget::isRunning()
{
	if (!m_streamServer)
		return false;
	return m_streamServer->isRunning();
}

void VideoWidget::setVideoFrame(QImage img)
{
	m_pixmap = QPixmap::fromImage(img).scaled(width() - 2, height() - 2, Qt::KeepAspectRatio);
	update();
}

void VideoWidget::setAudioFrame(QByteArray)
{

}

void VideoWidget::mousePressEvent(QMouseEvent* e)
{
	//m_mouseClickTimer->start(1000);
	clicked();
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
	//m_mouseClickTimer->stop();
	setFullScreen();
}

void VideoWidget::stop()
{
	m_streamServer->stop();
	m_streamServer->videoDecoder()->disconnect();
	m_streamServer->audioDecoder()->disconnect();
}

void VideoWidget::signalClick()
{
	clicked();
	m_mouseClickTimer->stop();
}

void VideoWidget::setFullScreen()
{
	m_bFullScreen = !m_bFullScreen;
	if (m_bFullScreen)
	{
		m_pos = pos();
		m_size = size();
		setWindowFlags(Qt::Window);
		showFullScreen();
	}
	else
	{
		hide();
		showNormal();
		setWindowFlags(Qt::SubWindow);
		move(m_pos);
		resize(m_size);
		show();
	}
}

void VideoWidget::paintEvent(QPaintEvent* e)
{
	QWidget::paintEvent(e);
	if (!m_pixmap.isNull())
	{
		int w = m_pixmap.width();
		int h = m_pixmap.height();
		int sl = (width() - 2) > w ? (width() - 2 - w) / 2 + 1 : 1;
		int st = (height() - 2) > h ? (height() - 2 - h) / 2 + 1 : 1;
		QPainter painter(this);
		painter.drawPixmap(sl, st, w, h, m_pixmap);
	}
}