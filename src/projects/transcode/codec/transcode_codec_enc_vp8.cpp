//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_codec_enc_vp8.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "TranscodeCodec"

OvenCodecImplAvcodecEncVP8::OvenCodecImplAvcodecEncVP8()
{
	avcodec_register_all();
	_pkt_buf.clear();
	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();
	_change_format = false;
	_coded_frame_count = 0;
	_coded_data_size = 0;
	_transcode_context = nullptr;

}

OvenCodecImplAvcodecEncVP8::~OvenCodecImplAvcodecEncVP8()
{
    avcodec_free_context(&_context);
    av_frame_free(&_frame);
    av_packet_free(&_pkt);
}

int OvenCodecImplAvcodecEncVP8::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	AVCodecParameters *origin_par = NULL;

    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
    if (!codec) {
        logte("Codec not found\n");
        return 1;
    }

	_context = avcodec_alloc_context3(codec);
	if (!_context) {
		logte("Could not allocate video codec context\n");
		return 1;
	} 

	// 인코딩 옵션 설정
	_context->bit_rate				= _transcode_context->GetVideoBitrate();

	_context->rc_max_rate = _context->rc_min_rate = _context->bit_rate;

	_context->rc_buffer_size		= _context->bit_rate * 2;

    _context->sample_aspect_ratio	= (AVRational){1, 1};

    _context->time_base 			= (AVRational){_transcode_context->GetVideoTimeBase().GetNum(), _transcode_context->GetVideoTimeBase().GetDen()};

    _context->framerate 			= av_d2q(_transcode_context->_video_frame_rate, AV_TIME_BASE);

    _context->gop_size 				= _transcode_context->GetGOP();

    _context->max_b_frames 			= 0;

    _context->pix_fmt 				= AV_PIX_FMT_YUV420P;

	_context->width					= _transcode_context->GetVideoWidth();

    _context->height				= _transcode_context->GetVideoHeight();

    _context->thread_count 			= 4;

    _context->qmin 					= 0;

    _context->qmax 					= 50;
    
    // 0: CBR, 100:VBR
    // _context->qcompress 				= 0;

    _context->rc_initial_buffer_occupancy = 500 * _context->bit_rate / 1000;
    
	_context->rc_buffer_size = 1000 * _context->bit_rate / 1000;
    
	 AVDictionary*   opts    = nullptr;

	av_dict_set(&opts, "quality", "realtime", AV_OPT_FLAG_ENCODING_PARAM);


	if (avcodec_open2(_context, codec, &opts) < 0) {
		logte("Could not open codec\n");
		return 1;
	}

	return 0;
}

void OvenCodecImplAvcodecEncVP8::sendbuf(std::unique_ptr<MediaBuffer> buf)
{
	_pkt_buf.push_back(std::move(buf));
}


std::pair<int, std::unique_ptr<MediaBuffer>> OvenCodecImplAvcodecEncVP8::recvbuf()
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
		logte("\r\nError receiving a packet for decoding : AVERROR_EOF\n");
		return std::make_pair(-1, nullptr);
	}
	else if(ret < 0) 
	{
		// copy
		// frame->linesize[0] * frame->height
		logte("Error receiving a packet for decoding : %d\n", ret);
		return std::make_pair(-1, nullptr);
	}
	else
	{
		_coded_frame_count++;
		_coded_data_size += _pkt->size;

#if 0
		if(_decoded_frame_num % 30 == 0)
			logti("encoded video packet pts=%10.0f size=%10d flags=%5d\n, encoded_size(%lld)",
			 (float)_pkt->pts, _pkt->size, _pkt->flags, _encoded_data_size);
#endif

		auto pbuf = std::make_unique<MediaBuffer>(MediaType::MEDIA_TYPE_VIDEO, 0, _pkt->data, _pkt->size, _pkt->dts, _pkt->flags);
		pbuf->SetWidth(_context->width);
		pbuf->SetHeight(_context->height);

		av_packet_unref(_pkt);

		return std::make_pair(0, std::move(pbuf));
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
			logte("Could not allocate the video frame data\n");
			return std::make_pair(-1, nullptr);
		}
		
		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable\n");
			return std::make_pair(-1, nullptr);
		}

		// 데이터를 복사함.
		memcpy(_frame->data[0], cur_pkt->GetBuffer(0), cur_pkt->GetBufferSize(0));
		memcpy(_frame->data[1], cur_pkt->GetBuffer(1), cur_pkt->GetBufferSize(1));
		memcpy(_frame->data[2], cur_pkt->GetBuffer(2), cur_pkt->GetBufferSize(2));


		int ret = avcodec_send_frame(_context, _frame);
		if(ret < 0)
		{
			logte("Error sending a frame for encoding : %d\n", ret);
		}

		av_frame_unref(_frame);

		_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin()+1);
	}

	return std::make_pair(-1, nullptr);
}
