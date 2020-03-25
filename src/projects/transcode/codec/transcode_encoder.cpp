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

#include <utility>

#define OV_LOG_TAG "TranscodeCodec"

TranscodeEncoder::TranscodeEncoder()
{
	avcodec_register_all();

	_packet = ::av_packet_alloc();
	_frame = ::av_frame_alloc();

	_codec_par = ::avcodec_parameters_alloc();
}

TranscodeEncoder::~TranscodeEncoder()
{
	OV_SAFE_FUNC(_context, nullptr, ::avcodec_free_context, &);

	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_packet, nullptr, ::av_packet_free, &);

	OV_SAFE_FUNC(_codec_par, nullptr, ::avcodec_parameters_free, &);

	_input_buffer.clear();
	_output_buffer.clear();	
}

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::CreateEncoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> output_context)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;

	switch(codec_id)
	{
		case common::MediaCodecId::H264:
			encoder = std::make_shared<OvenCodecImplAvcodecEncAVC>();
			break;

		case common::MediaCodecId::Aac:
			encoder = std::make_shared<OvenCodecImplAvcodecEncAAC>();
			break;

		case common::MediaCodecId::Vp8:
			encoder = std::make_shared<OvenCodecImplAvcodecEncVP8>();
			break;

		case common::MediaCodecId::Opus:
			encoder = std::make_shared<OvenCodecImplAvcodecEncOpus>();
			break;

		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

	if(encoder != nullptr)
	{
        if (encoder->Configure(std::move(output_context)) == false)
        {
            return nullptr;
        }
	}

	return std::move(encoder);
}

bool TranscodeEncoder::Configure(std::shared_ptr<TranscodeContext> context)
{
	_output_context = context;

	return (_output_context != nullptr);
}

void TranscodeEncoder::SendBuffer(std::shared_ptr<const MediaFrame> frame)
{
	std::unique_lock<std::mutex> mlock(_mutex);

	_input_buffer.push_back(std::move(frame));
	
	mlock.unlock();
	
	_queue_event.Notify();;
}

std::shared_ptr<TranscodeContext>& TranscodeEncoder::GetContext()
{
	return _output_context;
}

void TranscodeEncoder::ThreadEncode()
{
	// nothing...
}

void TranscodeEncoder::Stop()
{
	// nothing...
}