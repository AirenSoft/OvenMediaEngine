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

int OvenCodecImplAvcodecEncAAC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	// find the MPEG-1 video decoder
	AVCodec *codec = avcodec_find_encoder(GetCodecID());
	if(!codec)
	{
		printf("Codec not found\n");
		return 1;
	}

	// create codec context
	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		printf("Could not allocate audio codec context\n");
		return 1;
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
		printf("Could not open codec\n");
		return 1;
	}

	return 0;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAAC::RecvBuffer(TranscodeResult *result)
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
		printf("\r\nError receiving a packet for decoding : AVERROR_EOF\n");

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if(ret < 0)
	{
		printf("Error receiving a packet for encoding : %d\n", ret);

		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		printf("encoded audio packet %10.0f size=%10d flags=%5d\n", (float)_pkt->pts, _pkt->size, _pkt->flags);

		// Utils::Debug::DumpHex(_pkt->data, (_pkt->size>32)?32:_pkt->size);

		*result = TranscodeResult::DataReady;
		return nullptr;
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while(_input_buffer.size() > 0)
	{
		const MediaFrame *cur_pkt = _input_buffer[0].get();

		_frame->nb_samples = _context->frame_size;
		_frame->format = _context->sample_fmt;
		_frame->channel_layout = _context->channel_layout;
		_frame->pts = cur_pkt->GetPts();

		if(av_frame_get_buffer(_frame, 0) < 0)
		{
			printf("Could not allocate the audio frame data\n");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			printf("Could not make sure the frame data is writable\n");

			*result = TranscodeResult::DataError;
			return nullptr;
		}


		int ret = avcodec_send_frame(_context, _frame);
		if(ret < 0)
		{
			printf("Error sending a frame for encoding : %d\n", ret);
		}

		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
