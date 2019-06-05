//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_vp8.h"

#define OV_LOG_TAG "TranscodeCodec"

bool OvenCodecImplAvcodecEncVP8::Configure(std::shared_ptr<TranscodeContext> context)
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

	// 인코딩 옵션 설정
	_context->bit_rate = _transcode_context->GetBitrate();
	_context->rc_max_rate = _context->rc_min_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate * 2);
	_context->sample_aspect_ratio = (AVRational){ 1, 1 };
	_context->time_base = (AVRational){
		_transcode_context->GetTimeBase().GetNum(), _transcode_context->GetTimeBase().GetDen()
	};
	_context->framerate = av_d2q(_transcode_context->GetFrameRate(), AV_TIME_BASE);
	_context->gop_size = _transcode_context->GetGOP();
	_context->max_b_frames = 0;
	_context->pix_fmt = AV_PIX_FMT_YUV420P;
	_context->width = _transcode_context->GetVideoWidth();
	_context->height = _transcode_context->GetVideoHeight();
	_context->thread_count = 4;
	_context->qmin = 0;
	_context->qmax = 50;
	// 0: CBR, 100:VBR
	// _context->qcompress 				= 0;
	_context->rc_initial_buffer_occupancy = static_cast<int>(500 * _context->bit_rate / 1000);
	_context->rc_buffer_size = static_cast<int>(1000 * _context->bit_rate / 1000);

	AVDictionary *opts = nullptr;

	av_dict_set(&opts, "quality", "realtime", AV_OPT_FLAG_ENCODING_PARAM);

	if(avcodec_open2(_context, codec, &opts) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncVP8::RecvBuffer(TranscodeResult *result)
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
		_coded_frame_count++;
		_coded_data_size += _pkt->size;

#if 0
		if(_decoded_frame_num % 30 == 0)
			logti("encoded video packet pts=%10.0f size=%10d flags=%5d\n, encoded_size(%lld)",
			 (float)_pkt->pts, _pkt->size, _pkt->flags, _encoded_data_size);
#endif

		auto packet_buffer = std::make_unique<MediaPacket>(common::MediaType::Video, 0, _pkt->data, _pkt->size, _pkt->dts, (_pkt->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

		av_packet_unref(_pkt);

		*result = TranscodeResult::DataReady;
		return std::move(packet_buffer);
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while(_input_buffer.size() > 0)
	{
		auto frame_buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = frame_buffer.get();
		OV_ASSERT2(frame != nullptr);

		_frame->format = frame->GetFormat();
		_frame->width = frame->GetWidth();
		_frame->height = frame->GetHeight();
		_frame->pts = frame->GetPts();

		_frame->linesize[0] = frame->GetStride(0);
		_frame->linesize[1] = frame->GetStride(1);
		_frame->linesize[2] = frame->GetStride(2);

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

		// Copy packet data into frame
		memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		int ret = avcodec_send_frame(_context, _frame);

		if(ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
			// TODO(soulk): 에러 처리 안해도 되는지?
		}

		av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
