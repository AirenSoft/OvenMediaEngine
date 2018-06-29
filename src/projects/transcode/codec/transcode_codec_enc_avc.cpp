//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "transcode_codec_enc_avc.h"
#include <base/ovlibrary/ovlibrary.h>


#define OV_LOG_TAG "TranscodeCodec"

OvenCodecImplAvcodecEncAVC::OvenCodecImplAvcodecEncAVC()
{
	avcodec_register_all();

	_pkt_buf.clear();
	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();
	_codec_par = avcodec_parameters_alloc();
	_change_format = false;
	
	_coded_frame_count = 0;
	_coded_data_size = 0;

}

OvenCodecImplAvcodecEncAVC::~OvenCodecImplAvcodecEncAVC()
{
    avcodec_free_context(&_context);
    avcodec_parameters_free(&_codec_par);
    av_frame_free(&_frame);
    av_packet_free(&_pkt);
}

int OvenCodecImplAvcodecEncAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	// AVCodecParameters *origin_par = NULL;

    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        logte("Codec not found\n");
        return 1;
    }

	_context = avcodec_alloc_context3(codec);
	if (!_context) {
		logte("Could not allocate video codec context\n");
		return 1;
	} 

	_context->bit_rate				= _transcode_context->_video_bitrate;

	_context->rc_max_rate = _context->rc_min_rate = _context->bit_rate;

	_context->rc_buffer_size		= _context->bit_rate * 2;

    _context->sample_aspect_ratio	= (AVRational){1, 1};

    _context->time_base 			= (AVRational){1, AV_TIME_BASE};

    _context->framerate 			= av_d2q(_transcode_context->_video_frame_rate, AV_TIME_BASE);

    _context->gop_size 				= _transcode_context->_video_gop;

    _context->max_b_frames 			= 0;

    _context->pix_fmt 				= AV_PIX_FMT_YUV420P;

	_context->width					= _transcode_context->_video_width;

    _context->height				=  _transcode_context->_video_height;

    _context->thread_count 			= 4;


    if(codec->id == AV_CODEC_ID_H264)
        av_opt_set(_context->priv_data, "preset", "fast", 0);

	// open codec
	if(avcodec_open2(_context, codec, NULL) < 0) {
		logte("Could not open codec\n");
		return 1;
	}	

	return 0;
}


void OvenCodecImplAvcodecEncAVC::sendbuf(std::unique_ptr<MediaBuffer> buf)
{
	_pkt_buf.push_back(std::move(buf));
}

std::pair<int, std::unique_ptr<MediaBuffer>> OvenCodecImplAvcodecEncAVC::recvbuf()
{
	int ret;

	///////////////////////////////////////////////////
	// 디코딩 가능한 프레임이 존재하는지 확인한다.
	///////////////////////////////////////////////////
	ret = avcodec_receive_packet(_context, _pkt);
	if(ret == AVERROR(EAGAIN) )
	{
		// 패킷을 넣음
	}
	else if(ret == AVERROR_EOF)
	{
		printf("\r\nError receiving a packet for decoding : AVERROR_EOF\n");
		return std::make_pair(-1, nullptr);
	}
	else if(ret < 0) 
	{
		// copy
		// frame->linesize[0] * frame->height
		printf("Error receiving a packet for decoding : %d\n", ret);
		return std::make_pair(-1, nullptr);
	}
	else
	{
		printf("encoded video packet %10.0f size=%10d flags=%5d\n", (float)_pkt->pts, _pkt->size, _pkt->flags);

		// Utils::Debug::DumpHex(_pkt->data, (_pkt->size>80)?80:_pkt->size);

		return std::make_pair(0, nullptr);
	}


	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while (_pkt_buf.size() > 0) {

		MediaBuffer* cur_pkt = _pkt_buf[0].get();

		_frame->format = cur_pkt->_format;
		_frame->width  = cur_pkt->_width;
		_frame->height = cur_pkt->_height;
		_frame->pts    = cur_pkt->GetPts();

		if(av_frame_get_buffer(_frame, 32) < 0)
		{
			printf("Could not allocate the video frame data\n");
			return std::make_pair(-1, nullptr);
		}
		
		if(av_frame_make_writable(_frame) < 0)
		{
			printf("Could not make sure the frame data is writable\n");
				return std::make_pair(-1, nullptr);
		}

		int ret = avcodec_send_frame(_context, _frame);
		if(ret < 0)
		{
			printf("Error sending a frame for encoding : %d\n", ret);
		}

		av_frame_unref(_frame);

		_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin()+1);
	}

	return std::make_pair(-1, nullptr);
}
