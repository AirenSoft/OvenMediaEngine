//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_encoder.h"

#include <utility>

#include "codec/encoder/encoder_aac.h"
#include "codec/encoder/encoder_avc.h"
#include "codec/encoder/encoder_avc_qsv.h"
#include "codec/encoder/encoder_hevc.h"
#include "codec/encoder/encoder_hevc_qsv.h"
#include "codec/encoder/encoder_jpeg.h"
#include "codec/encoder/encoder_opus.h"
#include "codec/encoder/encoder_png.h"
#include "codec/encoder/encoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 120
#define HWACCEL_ENABLE

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

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::CreateEncoder(std::shared_ptr<TranscodeContext> context)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;

	bool use_hwaceel = context->GetHardwareAccel();
	logtd("Use hardware accelerator for encdoer is %s", use_hwaceel ? "enabled" : "disabled");

	switch (context->GetCodecId())
	{
		case cmn::MediaCodecId::H264:
#ifdef HWACCEL_ENABLE
			if (use_hwaceel == true && TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
			{
				encoder = std::make_shared<EncoderAVCxQSV>();
				if (encoder != nullptr && encoder->Configure(context) == true)
				{
					return encoder;
				}
			}
#endif
			encoder = std::make_shared<EncoderAVC>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::H265:
#ifdef HWACCEL_ENABLE
			if (use_hwaceel == true && TranscodeGPU::GetInstance()->IsSupportedQSV() == true)
			{
				encoder = std::make_shared<EncoderHEVCxQSV>();
				if (encoder != nullptr && encoder->Configure(context) == true)
				{
					return encoder;
				}
			}
#endif
			encoder = std::make_shared<EncoderHEVC>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::Vp8:
			encoder = std::make_shared<EncoderVP8>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::Jpeg:
			encoder = std::make_shared<EncoderJPEG>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::Png:
			encoder = std::make_shared<EncoderPNG>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::Aac:
			encoder = std::make_shared<EncoderAAC>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		case cmn::MediaCodecId::Opus:
			encoder = std::make_shared<EncoderOPUS>();
			if (encoder != nullptr && encoder->Configure(context) == true)
			{
				return encoder;
			}

			break;
		default:
			OV_ASSERT(false, "Not supported codec: %d", context->GetCodecId());
			break;
	}

	return nullptr;
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

	_input_buffer.SetAlias(ov::String::FormatString("Input queue of Encoder. codec(%s/%d)", ::avcodec_get_name(GetCodecID()), GetCodecID()));
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);
	_output_buffer.SetAlias(ov::String::FormatString("Output queue of Encoder. codec(%s/%d)", ::avcodec_get_name(GetCodecID()), GetCodecID()));
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