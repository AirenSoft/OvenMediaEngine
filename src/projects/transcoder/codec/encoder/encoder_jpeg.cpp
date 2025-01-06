//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_jpeg.h"

#include <unistd.h>

#include <fstream>

#include "../../transcoder_private.h"

bool EncoderJPEG::SetCodecParams()
{
	_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	_codec_context->framerate = ::av_d2q((GetRefTrack()->GetFrameRateByConfig() > 0) ? GetRefTrack()->GetFrameRateByConfig() : GetRefTrack()->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->time_base = ffmpeg::Conv::TimebaseToAVRational(GetRefTrack()->GetTimeBase());
	_codec_context->pix_fmt = (AVPixelFormat)GetSupportedFormat();
	_codec_context->width = GetRefTrack()->GetWidth();
	_codec_context->height = GetRefTrack()->GetHeight();
	_codec_context->flags = AV_CODEC_FLAG_QSCALE;
	_codec_context->global_quality = _codec_context->qmin * FF_QP2LAMBDA;

	// Set color range to JPEG
	_codec_context->color_range = AVCOL_RANGE_JPEG;
	_codec_context->strict_std_compliance = FF_COMPLIANCE_STRICT;

	_bitstream_format = GetBitstreamFormat();
	
	_packet_type = cmn::PacketType::RAW;

	return true;
}

bool EncoderJPEG::InitCodec()
{
	auto codec_id = GetCodecID();

	const AVCodec *codec = ::avcodec_find_encoder(codec_id);
	if (codec == nullptr)
	{
		logte("Could not find encoder: %d (%s)", codec_id, ::avcodec_get_name(codec_id));
		return false;
	}

	_codec_context = ::avcodec_alloc_context3(codec);
	if (_codec_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	if (SetCodecParams() == false)
	{
		logte("Could not set codec parameters for %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	if (::avcodec_open2(_codec_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	return true;
}

bool EncoderJPEG::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderJPEG::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("ENC-%s-t%d", avcodec_get_name(GetCodecID()), _track->GetId()).CStr());
		
		// Initialize the codec and wait for completion.
		if(_codec_init_event.Get() == false)
		{
			_kill_flag = true;
			return false;
		}
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		return false;
	}
	return true;
}
