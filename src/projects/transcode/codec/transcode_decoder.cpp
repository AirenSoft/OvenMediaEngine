//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_decoder.h"

#include "transcode_codec_dec_aac.h"
#include "transcode_codec_dec_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

TranscodeDecoder::TranscodeDecoder()
{
	avcodec_register_all();

	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();

}

TranscodeDecoder::~TranscodeDecoder()
{
	avcodec_free_context(&_context);
	avcodec_parameters_free(&_codec_par);

	av_frame_free(&_frame);
	av_packet_free(&_pkt);

	av_parser_close(_parser);
}

std::unique_ptr<TranscodeDecoder> TranscodeDecoder::CreateDecoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> transcode_context)
{
	std::unique_ptr<TranscodeDecoder> decoder = nullptr;

	switch(codec_id)
	{
		case common::MediaCodecId::H264:
			decoder = std::make_unique<OvenCodecImplAvcodecDecAVC>();
			break;

		case common::MediaCodecId::Aac:
			decoder = std::make_unique<OvenCodecImplAvcodecDecAAC>();
			break;

		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

	if(decoder != nullptr)
	{
		decoder->Configure(transcode_context);
	}

	return std::move(decoder);
}

bool TranscodeDecoder::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	_codec = avcodec_find_decoder(GetCodecID());

	if(_codec == nullptr)
	{
		logte("Codec not found");
		return false;
	}

	// create codec context
	_context = avcodec_alloc_context3(_codec);

	if(_context == nullptr)
	{
		logte("Could not allocate video codec context");
		return false;
	}

	if(avcodec_open2(_context, _codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	_parser = av_parser_init(_codec->id);

	if(!_parser)
	{
		logte("Parser not found");
		return false;
	}

	return true;
}

void TranscodeDecoder::SendBuffer(std::unique_ptr<const MediaPacket> packet)
{
	_input_buffer.push_back(std::move(packet));
}
