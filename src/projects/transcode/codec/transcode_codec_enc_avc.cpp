//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

bool OvenCodecImplAvcodecEncAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	AVCodec *codec = avcodec_find_encoder(GetCodecID());
	if(!codec)
	{
		logte("Codec not found");
		return false;
	}

	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		logte("Could not allocate video codec context");
		return false;
	}

	_context->bit_rate = _transcode_context->GetVideoBitrate();
	_context->rc_max_rate = _context->rc_min_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate * 2);
	_context->sample_aspect_ratio = (AVRational){ 1, 1 };
	_context->time_base = (AVRational){ 1, AV_TIME_BASE };
	_context->framerate = av_d2q(_transcode_context->GetFrameRate(), AV_TIME_BASE);
	_context->gop_size = _transcode_context->GetGOP();
	_context->max_b_frames = 0;
	_context->pix_fmt = AV_PIX_FMT_YUV420P;
	_context->width = _transcode_context->GetVideoWidth();
	_context->height = _transcode_context->GetVideoHeight();
	_context->thread_count = 4;

	av_opt_set(_context->priv_data, "preset", "fast", 0);

	// open codec
	if(avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAVC::RecvBuffer(TranscodeResult *result)
{
	int ret;

	///////////////////////////////////////////////////
	// 디코딩 가능한 프레임이 존재하는지 확인한다.
	///////////////////////////////////////////////////
	ret = avcodec_receive_packet(_context, _pkt);
	if(ret == AVERROR(EAGAIN))
	{
		// 패킷을 넣음
	}
	else if(ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if(ret < 0)
	{
		// copy
		// frame->linesize[0] * frame->height
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		logtd("encoded video packet %10.0f size=%10d flags=%5d", (float)_pkt->pts, _pkt->size, _pkt->flags);

		// Utils::Debug::DumpHex(_pkt->data, (_pkt->size>80)?80:_pkt->size);

		// TODO(soulk): 여기서 데이터를 안넘겨도 되는지 확인
		*result = TranscodeResult::DataReady;
		return nullptr;
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while(_input_buffer.size() > 0)
	{
		const MediaFrame *cur_pkt = _input_buffer[0].get();

		_frame->format = cur_pkt->GetFormat();
		_frame->width = cur_pkt->GetWidth();
		_frame->height = cur_pkt->GetHeight();
		_frame->pts = cur_pkt->GetPts();

		if(av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		ret = avcodec_send_frame(_context, _frame);

		if(ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		av_frame_unref(_frame);

		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
