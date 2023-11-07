//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_avc_xma.h"

#include <unistd.h>

#include "../../transcoder_private.h"

bool EncoderAVCxXMA::SetCodecParams()
{
	_codec_context->framerate = ::av_d2q((GetRefTrack()->GetFrameRate() > 0) ? GetRefTrack()->GetFrameRate() : GetRefTrack()->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->bit_rate = _codec_context->rc_min_rate = _codec_context->rc_max_rate = GetRefTrack()->GetBitrate();
	_codec_context->sample_aspect_ratio = ::av_make_q(1, 1);
	_codec_context->ticks_per_frame = 2;
	_codec_context->time_base = ::av_inv_q(::av_mul_q(_codec_context->framerate, (AVRational){_codec_context->ticks_per_frame, 1}));
	_codec_context->pix_fmt = (AVPixelFormat)GetSupportedFormat();
	_codec_context->width = GetRefTrack()->GetWidth();
	_codec_context->height = GetRefTrack()->GetHeight();

	// Control Rate
	::av_opt_set(_codec_context->priv_data, "control-rate", "cbr", 0); // low-latency

	// Bit-rate
	::av_opt_set_int(_codec_context->priv_data, "max-bitrate",  _codec_context->bit_rate, 0);

	// KeyFrame Interval
	_codec_context->gop_size = (GetRefTrack()->GetKeyFrameInterval() == 0) ? (_codec_context->framerate.num / _codec_context->framerate.den) : GetRefTrack()->GetKeyFrameInterval();
	::av_opt_set_int(_codec_context->priv_data, "periodicity-idr",  _codec_context->gop_size, 0);

	// Bframes
	::av_opt_set_int(_codec_context->priv_data, "bf", GetRefTrack()->GetBFrames(), 0);

	// Profile
	auto profile = GetRefTrack()->GetProfile();
	if (profile == "baseline" || profile.IsEmpty() == true)
	{
		::av_opt_set(_codec_context->priv_data, "profile", "baseline", 0);
		::av_opt_set(_codec_context->priv_data, "expert-options", "entropy-mode=0", 0);
	}
	else if (profile == "main")
	{
		::av_opt_set(_codec_context->priv_data, "profile", "high", 0);
	}
	else if (profile == "high")
	{
		::av_opt_set(_codec_context->priv_data, "profile", "high", 0);
	}
	else
	{
		logtw("This is an unknown profile. change to the default(baseline) profile.");
		::av_opt_set(_codec_context->priv_data, "profile", "baseline", 0);
		::av_opt_set(_codec_context->priv_data, "expert-options", "entropy-mode=0", 0);
	}

	::av_opt_set(_codec_context->priv_data, "level", "4.2", 0);
	::av_opt_set(_codec_context->priv_data, "scaling-list", "flat", 0);
	
	// Enable AVC low latency flag for H264 to run on multiple cores incase of pipeline disabled
	::av_opt_set(_codec_context->priv_data, "avc-lowlat", "enable", 0);
	::av_opt_set(_codec_context->priv_data, "disable-pipeline", "enable", 0);
	
	::av_opt_set_int(_codec_context->priv_data, "lxlnx_hwdev", GetRefTrack()->GetCodecDeviceId(), 0);

	auto preset = GetRefTrack()->GetPreset().LowerCaseString();
	if (preset.IsEmpty() == false)
	{
		logtd("Xilinx encoder does not support preset");
	}

	return true;
}

bool EncoderAVCxXMA::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}
	
	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&TranscodeEncoder::CodecThread, this);
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

void EncoderAVCxXMA::CodecThread()
{
	ov::String codec_name = "mpsoc_vcu_h264";
	
	const AVCodec *codec = ::avcodec_find_encoder_by_name(codec_name.CStr());
	if (codec == nullptr)
	{
		logte("Could not find encoder: %s", codec_name.CStr());
		return ;
	}

	_codec_context = ::avcodec_alloc_context3(codec);
	if (_codec_context == nullptr)
	{
		logte("Could not allocate codec context for %s", codec_name.CStr());
		return ;
	}

	if (SetCodecParams() == false)
	{
		logte("Could not set codec parameters for %s", codec_name.CStr());
		return ;
	}

	if (::avcodec_open2(_codec_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s", codec_name.CStr());
		return ;
	}

	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
			continue;

		auto media_frame = std::move(obj.value());

		///////////////////////////////////////////////////
		// Request frame encoding to codec
		///////////////////////////////////////////////////
		auto av_frame = ffmpeg::Conv::ToAVFrame(cmn::MediaType::Video, media_frame);
		if (!av_frame)
		{
			logte("Could not allocate the video frame data");
			break;
		}

		int ret = ::avcodec_send_frame(_codec_context, av_frame);
		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		///////////////////////////////////////////////////
		// The encoded packet is taken from the codec.
		///////////////////////////////////////////////////
		while (true)
		{
			// Check frame is available
			int ret = ::avcodec_receive_packet(_codec_context, _packet);
			if (ret == AVERROR(EAGAIN))
			{
				// More packets are needed for encoding.
				break;
			}
			else if (ret == AVERROR_EOF && ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
				auto media_packet = ffmpeg::Conv::ToMediaPacket(_packet, cmn::MediaType::Video, cmn::BitstreamFormat::H264_ANNEXB, cmn::PacketType::NALU);
				if (media_packet == nullptr)
				{
					logte("Could not allocate the media packet");
					break;
				}

				::av_packet_unref(_packet);

				SendOutputBuffer(std::move(media_packet));
			}
		}
	}
}
