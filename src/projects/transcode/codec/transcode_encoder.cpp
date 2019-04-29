#include <utility>

//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_encoder.h"

#include "transcode_codec_enc_avc.h"
#include "transcode_codec_enc_aac.h"
#include "transcode_codec_enc_vp8.h"
#include "transcode_codec_enc_opus.h"

#define OV_LOG_TAG "TranscodeCodec"

TranscodeEncoder::TranscodeEncoder()
{
	avcodec_register_all();

	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();

	_codec_par = avcodec_parameters_alloc();
}

TranscodeEncoder::~TranscodeEncoder()
{
	OV_SAFE_FUNC(_context, nullptr, avcodec_free_context, &);

	OV_SAFE_FUNC(_frame, nullptr, av_frame_free, &);
	OV_SAFE_FUNC(_pkt, nullptr, av_packet_free, &);

	OV_SAFE_FUNC(_codec_par, nullptr, avcodec_parameters_free, &);
}

std::unique_ptr<TranscodeEncoder> TranscodeEncoder::CreateEncoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> transcode_context)
{
	std::unique_ptr<TranscodeEncoder> encoder = nullptr;

	switch(codec_id)
	{
		case common::MediaCodecId::H264:
			encoder = std::make_unique<OvenCodecImplAvcodecEncAVC>();
			break;

		case common::MediaCodecId::Aac:
			encoder = std::make_unique<OvenCodecImplAvcodecEncAAC>();
			break;

		case common::MediaCodecId::Vp8:
			encoder = std::make_unique<OvenCodecImplAvcodecEncVP8>();
			break;

		case common::MediaCodecId::Opus:
			encoder = std::make_unique<OvenCodecImplAvcodecEncOpus>();
			break;

		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

	if(encoder != nullptr)
	{
        if (!encoder->Configure(std::move(transcode_context)))
        {
            return nullptr;
        }
	}

	return std::move(encoder);
}

bool TranscodeEncoder::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	return (_transcode_context != nullptr);
}

void TranscodeEncoder::SendBuffer(std::unique_ptr<const MediaFrame> frame)
{
	_input_buffer.push_back(std::move(frame));
}