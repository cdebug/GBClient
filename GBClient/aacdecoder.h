#ifndef AACDECODER_H
#define AACDECODER_H
#include <QObject>

class AACDecoder :public QObject
{
	Q_OBJECT
public:
	AACDecoder();
	~AACDecoder();
	void aac_decoder_create(int sample_rate, int channels, int bit_rate);
	int aac_decode_frame(uint8_t *pData, int nLen);
	void aac_decode_close();
private:
	struct AVCodecContext *m_pCodecCtx;
	struct AVFrame *m_pFrame;
	struct SwrContext *m_au_convert_ctx;
	int m_out_buffer_size;
signals:
	void frameChanged(QByteArray);
};

#endif //AACDECODER_H