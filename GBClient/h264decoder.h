#ifndef H264DECODER_H
#define H264DECODER_H
#include <QImage>

class H264Decoder : public QObject
{
	Q_OBJECT
public:
	H264Decoder();
	~H264Decoder();
	bool init();
	void decodeFrame(uint8_t* buffer, uint32_t bufferLen);

private:
	struct AVCodecContext* _pCodecContext;
	struct AVCodec* _pH264VideoDecoder;
	struct AVFrame	*_pFrame, *_pFrameRGB;
	struct SwsContext *_imgConvertCtx;
signals:
	void frameChanged(QImage);
};

#endif //H264DECODER_H