#include "aacdecoder.h"
extern "C"
{
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
}

#define MAX_AUDIO_FRAME_SIZE 192000

AACDecoder::AACDecoder()
{
}


AACDecoder::~AACDecoder()
{
}


void AACDecoder::aac_decoder_create(int sample_rate, int channels, int bit_rate)
{
	AVCodec *pCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
	if (pCodec == NULL)
	{
		printf("find aac decoder error\r\n");
		return ;
	}
	// ´´½¨ÏÔÊ¾contedxt
	m_pCodecCtx = avcodec_alloc_context3(pCodec);
	if (m_pCodecCtx == nullptr)
	{
		printf("can not alloc codecContext\r\n");
		return;
	}
	m_pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_pCodecCtx->frame_number = 1;
	m_pCodecCtx->sample_rate = sample_rate;
	m_pCodecCtx->channels = channels;
	m_pCodecCtx->bit_rate = bit_rate;
	m_pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	if (avcodec_open2(m_pCodecCtx, pCodec, NULL) < 0)
	{
		printf("open codec error\r\n");
		return ;
	}
	m_pFrame = av_frame_alloc();
	uint64_t out_channel_layout = channels < 2 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
	int out_nb_samples = 1024;
	enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

	m_au_convert_ctx = swr_alloc();
	m_au_convert_ctx = swr_alloc_set_opts(m_au_convert_ctx, out_channel_layout, out_sample_fmt, sample_rate,
		out_channel_layout, AV_SAMPLE_FMT_FLTP, sample_rate, 0, NULL);
	swr_init(m_au_convert_ctx);
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	m_out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
}

int AACDecoder::aac_decode_frame(uint8_t *pData, int nLen)
{
	AVPacket packet;
	av_init_packet(&packet);

	packet.size = nLen;
	packet.data = pData;
	uint8_t *out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	if (packet.size > 0)
	{
		int ret = avcodec_send_packet(m_pCodecCtx, &packet);
		int got_picture = avcodec_receive_frame(m_pCodecCtx, m_pFrame);
		if (ret >= 0)
		{
			if (got_picture == 0)
			{
				ret = swr_convert(m_au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)m_pFrame->data, m_pFrame->nb_samples);
				frameChanged(QByteArray((char*)out_buffer, m_out_buffer_size));
			}
		}
		else
			printf("avcodec_decode_audio4 %d  sameles = %d  outSize = %d\r\n", ret, m_pFrame->nb_samples, m_out_buffer_size);
	}
	av_free(out_buffer);
	av_free_packet(&packet);
	return 0;
}

void AACDecoder::aac_decode_close()
{
	swr_free(&m_au_convert_ctx);

	if (m_pFrame != NULL)
	{
		av_frame_free(&m_pFrame);
		m_pFrame = NULL;
	}

	if (m_pCodecCtx != NULL)
	{
		avcodec_close(m_pCodecCtx);
		avcodec_free_context(&m_pCodecCtx);
		m_pCodecCtx = NULL;
	}
}