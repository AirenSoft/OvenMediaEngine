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

EncoderVP8::~EncoderVP8()
{
}

bool EncoderVP8::SetCodecParams()
{
	_codec_context->bit_rate = _encoder_context->GetBitrate();
	_codec_context->rc_max_rate = _codec_context->bit_rate;
	_codec_context->rc_min_rate = _codec_context->bit_rate;
	_codec_context->sample_aspect_ratio = (AVRational){1, 1};
	_codec_context->time_base = TimebaseToAVRational(_encoder_context->GetTimeBase());
	_codec_context->framerate = ::av_d2q((_encoder_context->GetFrameRate() > 0) ? _encoder_context->GetFrameRate() : _encoder_context->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->gop_size = _codec_context->framerate.num / _codec_context->framerate.den;
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetPixelFormat();
	_codec_context->width = _encoder_context->GetVideoWidth();
	_codec_context->height = _encoder_context->GetVideoHeight();

	// Limit the number of threads suitable for vp8 encoding to between 4 and 8.
	_codec_context->thread_count = (_encoder_context->GetThreadCount() > 0) ? _encoder_context->GetThreadCount() : FFMIN(FFMAX(4, av_cpu_count() / 3), 8);

	// Preset
	if (_encoder_context->GetPreset() == "slower" || _encoder_context->GetPreset() == "slow")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "best", 0);
	}
	else if (_encoder_context->GetPreset() == "medium")
	{
		::av_opt_set(_codec_context->priv_data, "quality", "good", 0);
	}
	else if (_encoder_context->GetPreset() == "fast" || _encoder_context->GetPreset() == "faster")
	{
		::av_opt_set(_codec_context->priv_data, "quality", "realtime", 0);
	}
	else
	{
		// Default
		::av_opt_set(_codec_context->priv_data, "quality", "realtime", 0);
	}

	return true;
}

bool EncoderVP8::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

	AVCodec *codec = ::avcodec_find_encoder(codec_id);
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

void EncoderVP8::CodecThread()
{
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
			logte("Could not allocate the frame data");
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
			// Check frame is availble
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
				auto media_packet = ffmpeg::Conv::ToMediaPacket(_packet, cmn::MediaType::Video, cmn::BitstreamFormat::VP8, cmn::PacketType::RAW);
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
