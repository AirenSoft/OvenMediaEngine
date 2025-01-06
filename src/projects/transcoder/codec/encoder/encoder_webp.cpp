//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_webp.h"

#include <unistd.h>

#include <fstream>

#include "../../transcoder_private.h"

bool EncoderWEBP::SetCodecParams()
{
	_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	_codec_context->framerate = ::av_d2q((GetRefTrack()->GetFrameRateByConfig() > 0) ? GetRefTrack()->GetFrameRateByConfig() : GetRefTrack()->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->time_base = ffmpeg::Conv::TimebaseToAVRational(GetRefTrack()->GetTimeBase());
	_codec_context->pix_fmt = (AVPixelFormat)GetSupportedFormat();
	_codec_context->width = GetRefTrack()->GetWidth();
	_codec_context->height = GetRefTrack()->GetHeight();

	// Set the compression level
	_codec_context->compression_level = 1;

	// Set the preset
	auto preset = GetRefTrack()->GetPreset();
	if (preset.IsEmpty())
	{
		::av_opt_set(_codec_context->priv_data, "preset", "default", 0);
	}
	else
	{
		if (preset == "none" ||
			preset == "default" ||
			preset == "picture" ||
			preset == "photo" ||
			preset == "drawing" ||
			preset == "icon" ||
			preset == "text")
		{
			::av_opt_set(_codec_context->priv_data, "preset", preset.CStr(), 0);
		}
	}

	_bitstream_format = GetBitstreamFormat();
	
	_packet_type = cmn::PacketType::RAW;

	return true;
}

bool EncoderWEBP::InitCodec()
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

bool EncoderWEBP::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderWEBP::CodecThread, this);
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

