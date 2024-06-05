//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_vp8.h"

#include "../../transcoder_private.h"

bool EncoderVP8::SetCodecParams()
{
	_codec_context->bit_rate = GetRefTrack()->GetBitrate();
	_codec_context->rc_max_rate = _codec_context->bit_rate;
	_codec_context->rc_min_rate = _codec_context->bit_rate;
	_codec_context->sample_aspect_ratio = (AVRational){1, 1};
	_codec_context->time_base = ffmpeg::Conv::TimebaseToAVRational(GetRefTrack()->GetTimeBase());
	_codec_context->framerate = ::av_d2q((GetRefTrack()->GetFrameRateByConfig() > 0) ? GetRefTrack()->GetFrameRateByConfig() : GetRefTrack()->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetSupportedFormat();
	_codec_context->width = GetRefTrack()->GetWidth();
	_codec_context->height = GetRefTrack()->GetHeight();

	// Keyframe Interval
	// @see transcoder_encoder.cpp / force_keyframe_by_time_interval
	auto key_frame_interval_type = GetRefTrack()->GetKeyFrameIntervalTypeByConfig();
	if (key_frame_interval_type == cmn::KeyFrameIntervalType::TIME)
	{
		_codec_context->gop_size = (int32_t)(GetRefTrack()->GetFrameRate() * (double)GetRefTrack()->GetKeyFrameInterval() / 1000 * 2);
	}
	else if (key_frame_interval_type == cmn::KeyFrameIntervalType::FRAME)
	{
		_codec_context->gop_size = (GetRefTrack()->GetKeyFrameInterval() == 0) ? (_codec_context->framerate.num / _codec_context->framerate.den) : GetRefTrack()->GetKeyFrameInterval();
	}
	// VP8 does not support bframe

	// -1(Default) => FFMIN(FFMAX(4, av_cpu_count() / 3), 8) 
	// 0 => Auto
	// >1 => Set
	_codec_context->thread_count = GetRefTrack()->GetThreadCount() < 0 ? FFMIN(FFMAX(4, av_cpu_count() / 3), 8) : GetRefTrack()->GetThreadCount();

	// Preset
	auto preset = GetRefTrack()->GetPreset().LowerCaseString();
	if (preset.IsEmpty() == true)
	{
		::av_opt_set(_codec_context->priv_data, "quality", "realtime", 0);
	}
	else
	{
		if (preset == "slower" || preset == "slow")
		{
			::av_opt_set(_codec_context->priv_data, "quality", "best", 0);
		}
		else if (preset == "medium")
		{
			::av_opt_set(_codec_context->priv_data, "quality", "good", 0);

		}		
		else if (preset == "fast" || preset == "faster")
		{
			::av_opt_set(_codec_context->priv_data, "quality", "realtime", 0);
		}
		else{
			logtw("Unknown preset: %s", preset.CStr());
		}
	}

	_bitstream_format = cmn::BitstreamFormat::VP8;
	
	_packet_type = cmn::PacketType::RAW;	

	return true;
}

bool EncoderVP8::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

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
		logte("Could not open codec");
		return false;
	}

	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderVP8::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("Enc%s", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start encoder thread.");
		_kill_flag = true;

		return false;
	}

	return true;
}

