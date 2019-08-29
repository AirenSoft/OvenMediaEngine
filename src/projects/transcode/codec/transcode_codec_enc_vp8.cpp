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
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

	AVCodec *codec = ::avcodec_find_encoder(codec_id);

	if (codec == nullptr)
	{
		logte("Could not find encoder: %d (%s)", codec_id, ::avcodec_get_name(codec_id));
		return false;
	}

	_context = ::avcodec_alloc_context3(codec);

	if (_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// 인코딩 옵션 설정
	_context->bit_rate = _output_context->GetBitrate();
	_context->rc_max_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate * 2);
	_context->sample_aspect_ratio = (AVRational){1, 1};
	_context->time_base = TimebaseToAVRational(_output_context->GetTimeBase());
	_context->framerate = ::av_d2q(_output_context->GetFrameRate(), AV_TIME_BASE);
	_context->gop_size = _output_context->GetGOP();
	_context->max_b_frames = 0;
	_context->pix_fmt = AV_PIX_FMT_YUV420P;
	_context->width = _output_context->GetVideoWidth();
	_context->height = _output_context->GetVideoHeight();
	_context->thread_count = 4;
	_context->qmin = 0;
	_context->qmax = 50;
	// 0: CBR, 100:VBR
	// _context->qcompress 				= 0;
	_context->rc_initial_buffer_occupancy = static_cast<int>(500 * _context->bit_rate / 1000);
	_context->rc_buffer_size = static_cast<int>(1000 * _context->bit_rate / 1000);

	AVDictionary *opts = nullptr;

	::av_dict_set(&opts, "quality", "realtime", AV_OPT_FLAG_ENCODING_PARAM);

	if (::avcodec_open2(_context, codec, &opts) < 0)
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
	ret = ::avcodec_receive_packet(_context, _packet);
	if (ret == AVERROR(EAGAIN))
	{
		// 패킷을 넣음
	}
	else if (ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if (ret < 0)
	{
		// copy
		// frame->linesize[0] * frame->height
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		// Packet is ready
		auto packet = MakePacket();
		::av_packet_unref(_packet);

		*result = TranscodeResult::DataReady;

		return std::move(packet);
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while (_input_buffer.size() > 0)
	{
		auto frame_buffer = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		const MediaFrame *frame = frame_buffer.get();
		OV_ASSERT2(frame != nullptr);

		_frame->format = frame->GetFormat();
		_frame->nb_samples = 1;
		_frame->pts = frame->GetPts();
		_frame->pkt_duration = frame->GetDuration();

		_frame->width = frame->GetWidth();
		_frame->height = frame->GetHeight();
		_frame->linesize[0] = frame->GetStride(0);
		_frame->linesize[1] = frame->GetStride(1);
		_frame->linesize[2] = frame->GetStride(2);

		if (::av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if (::av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		// Copy packet data into frame
		::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		::memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		::memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		int ret = ::avcodec_send_frame(_context, _frame);

		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
			// TODO(soulk): 에러 처리 안해도 되는지?
		}

		::av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncVP8::MakePacket() const
{
	auto flag = (_packet->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag;
	// This is workaround: avcodec_receive_packet() does not give the duration that sent to avcodec_send_frame()
	int den = _output_context->GetTimeBase().GetDen();
	int64_t duration = (den == 0) ? 0LL : (float)den / _output_context->GetFrameRate();
	auto packet = std::make_unique<MediaPacket>(common::MediaType::Video, 0, _packet->data, _packet->size, _packet->pts, _packet->dts, duration, flag);

	return std::move(packet);
}
