//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_aac.h"

#define OV_LOG_TAG "TranscodeCodec"

bool OvenCodecImplAvcodecEncAAC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	// find the MPEG-1 video decoder
	AVCodec *codec = avcodec_find_encoder(GetCodecID());
	if(!codec)
	{
		logte("Codec not found");
		return false;
	}

	// create codec context
	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		logte("Could not allocate audio codec context");
		return false;
	}

	// put sample parameters
	_context->bit_rate = 64000;

	// check that the encoder supports s16 pcm input
	// AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_NO
	_context->sample_fmt = AV_SAMPLE_FMT_FLT;

	// select other audio parameters supported by the encoder
	// 지원 가능한 샘플 레이트 48000, 24000, 16000, 12000, 8000, 0
	_context->sample_rate = 48000;
	_context->channel_layout = AV_CH_LAYOUT_STEREO;
	_context->channels = av_get_channel_layout_nb_channels(_context->channel_layout);

	_context->time_base = (AVRational){ 1, 1000 };

	// open codec
	if(avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAAC::RecvBuffer(TranscodeResult *result)
{
	// Check frame is availble
	int ret = avcodec_receive_packet(_context, _pkt);

	if(ret == AVERROR(EAGAIN))
	{
		// Packet is enqueued to encoder's buffer
	}
	else if(ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if(ret < 0)
	{
		logte("Error receiving a packet for encoding : %d", ret);

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		logtd("encoded audio packet %10.0f size=%10d flags=%5d", (float)_pkt->pts, _pkt->size, _pkt->flags);

		*result = TranscodeResult::DataReady;
		return nullptr;
	}

	// Try encode
	while(_input_buffer.empty() == false)
	{
		// Dequeue the frame
		auto buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = buffer.get();

		_frame->nb_samples = _context->frame_size;
		_frame->format = _context->sample_fmt;
		_frame->channel_layout = _context->channel_layout;
		_frame->pts = frame->GetPts();

		if(av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data");

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
			logte("Error sending a frame for encoding: %d", ret);
		}
	}

	return nullptr;
}
