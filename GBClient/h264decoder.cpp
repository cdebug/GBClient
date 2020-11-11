#include "h264decoder.h"
#include <QDebug>
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

FILE* fp_save;

H264Decoder::H264Decoder()
{
	fp_save = fopen("save.h264", "wb");
}

H264Decoder::~H264Decoder()
{
}

bool H264Decoder::init()
{
	av_register_all();
	_pCodecContext = avcodec_alloc_context3(NULL);
	_pH264VideoDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (_pH264VideoDecoder == NULL)
	{
		return false;
	}

	//初始化参数，下面的参数应该由具体的业务决定  AV_PIX_FMT_YUV420P;
	_pCodecContext->time_base.num = 1;
	_pCodecContext->frame_number = 1; //每包一个视频帧  
	_pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	_pCodecContext->bit_rate = 0;
	_pCodecContext->time_base.den = 25;//帧率  
	_pCodecContext->width = 0;//视频宽  
	_pCodecContext->height = 0;//视频高 
	_pCodecContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
	_pCodecContext->color_range = AVCOL_RANGE_MPEG;

	if (avcodec_open2(_pCodecContext, _pH264VideoDecoder, NULL) < 0)
		return false;
	_pFrame = av_frame_alloc();//存储解码后AVFrame
	_pFrameRGB = nullptr;
	int ret, got_picture;

	int y_size = _pCodecContext->width * _pCodecContext->height;
	AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));//存储解码前数据包AVPacket
	av_new_packet(packet, y_size);
	return true;
}

void H264Decoder::decodeFrame(uint8_t* buffer, uint32_t bufferLen)
{
	fwrite(buffer, 1, bufferLen, fp_save);
	AVPacket packet = { 0 };
	packet.data = buffer;    //这里填入一个指向完整H264数据帧的指针  
	packet.size = bufferLen;        //这个填入H264数据帧的大小  
	int ret = avcodec_send_packet(_pCodecContext, &packet);
	int got_picture = avcodec_receive_frame(_pCodecContext, _pFrame); //got_picture = 0 success, a frame was returned
	if (ret < 0)
	{
		return;
	}
	if (got_picture == 0)
	{
		//像素格式转换。pFrame转换为pFrameRGB。
		if (!_pFrameRGB)
		{
			_pFrameRGB = av_frame_alloc();
			uint8_t *video_buffer;
			video_buffer = new uint8_t[avpicture_get_size(AV_PIX_FMT_RGB32, _pCodecContext->width, _pCodecContext->height)];//分配AVFrame所需内存
			av_image_fill_arrays(_pFrameRGB->data, _pFrameRGB->linesize, video_buffer, AV_PIX_FMT_RGB32, _pCodecContext->width, _pCodecContext->height, 1);
			_imgConvertCtx = sws_getContext(_pCodecContext->width, _pCodecContext->height, _pCodecContext->pix_fmt,
				_pCodecContext->width, _pCodecContext->height, AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);
		}
		sws_scale(_imgConvertCtx, (const uint8_t* const*)_pFrame->data, _pFrame->linesize, 0, _pCodecContext->height, _pFrameRGB->data, _pFrameRGB->linesize);
		//------------显示--------
		QImage img((uchar*)_pFrameRGB->data[0], _pCodecContext->width, _pCodecContext->height, QImage::Format_RGB32);
		frameChanged(std::move(img));
	}
}