//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_encoder.h"

#include <utility>

#include "../transcode_private.h"
#include "transcode_codec_enc_aac.h"
#include "transcode_codec_enc_avc.h"
#include "transcode_codec_enc_hevc.h"
#include "transcode_codec_enc_jpeg.h"
#include "transcode_codec_enc_opus.h"
#include "transcode_codec_enc_png.h"
#include "transcode_codec_enc_vp8.h"

#define MAX_QUEUE_SIZE 120

TranscodeEncoder::TranscodeEncoder()
{
	_packet = ::av_packet_alloc();
	_frame = ::av_frame_alloc();

	_codec_par = ::avcodec_parameters_alloc();
}

TranscodeEncoder::~TranscodeEncoder()
{
	if (_context != nullptr)
		::avcodec_flush_buffers(_context);

	OV_SAFE_FUNC(_context, nullptr, ::avcodec_free_context, &);

	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_packet, nullptr, ::av_packet_free, &);

	OV_SAFE_FUNC(_codec_par, nullptr, ::avcodec_parameters_free, &);

	_input_buffer.Clear();
	_output_buffer.Clear();
}

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::CreateEncoder(cmn::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> output_context)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;

	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
			encoder = std::make_shared<OvenCodecImplAvcodecEncAVC>();
			break;
		case cmn::MediaCodecId::H265:
			encoder = std::make_shared<OvenCodecImplAvcodecEncHEVC>();
			break;
		case cmn::MediaCodecId::Vp8:
			encoder = std::make_shared<OvenCodecImplAvcodecEncVP8>();
			break;
		case cmn::MediaCodecId::Jpeg:
			encoder = std::make_shared<OvenCodecImplAvcodecEncJpeg>();
			break;
		case cmn::MediaCodecId::Png:
			encoder = std::make_shared<OvenCodecImplAvcodecEncPng>();
			break;
		case cmn::MediaCodecId::Aac:
			encoder = std::make_shared<OvenCodecImplAvcodecEncAAC>();
			break;
		case cmn::MediaCodecId::Opus:
			encoder = std::make_shared<OvenCodecImplAvcodecEncOpus>();
			break;
		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

	if (encoder != nullptr)
	{
		if (encoder->Configure(std::move(output_context)) == false)
		{
			return nullptr;
		}
	}

	return std::move(encoder);
}

cmn::Timebase TranscodeEncoder::GetTimebase() const
{
	return _output_context->GetTimeBase();
}

void TranscodeEncoder::SetTrackId(int32_t track_id)
{
	_track_id = track_id;
}

bool TranscodeEncoder::Configure(std::shared_ptr<TranscodeContext> context)
{
	_output_context = context;

	_input_buffer.SetAlias(ov::String::FormatString("Input queue of transcode encoder. codec(%s/%d)", ::avcodec_get_name(GetCodecID()), GetCodecID()));
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);
	_output_buffer.SetAlias(ov::String::FormatString("Output queue of transcode encoder. codec(%s/%d)", ::avcodec_get_name(GetCodecID()), GetCodecID()));
	_output_buffer.SetThreshold(MAX_QUEUE_SIZE);

	return (_output_context != nullptr);
}

void TranscodeEncoder::SendBuffer(std::shared_ptr<const MediaFrame> frame)
{
	_input_buffer.Enqueue(std::move(frame));
}

void TranscodeEncoder::SendOutputBuffer(std::shared_ptr<MediaPacket> packet)
{
	_output_buffer.Enqueue(std::move(packet));

	// Invoke callback function when encoding/decoding is completed.
	if (OnCompleteHandler)
		OnCompleteHandler(_track_id);
}

std::shared_ptr<TranscodeContext> &TranscodeEncoder::GetContext()
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