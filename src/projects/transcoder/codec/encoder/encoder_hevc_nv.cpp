//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_hevc_nv.h"

#include <unistd.h>

#include "../../transcoder_private.h"

EncoderHEVCxNV::~EncoderHEVCxNV()
{
}

bool EncoderHEVCxNV::SetCodecParams()
{
	_codec_context->framerate = ::av_d2q((_encoder_context->GetFrameRate() > 0) ? _encoder_context->GetFrameRate() : _encoder_context->GetEstimateFrameRate(), AV_TIME_BASE);
	_codec_context->bit_rate = _encoder_context->GetBitrate();
	_codec_context->rc_min_rate = _codec_context->rc_max_rate = _codec_context->bit_rate;
	_codec_context->rc_buffer_size = static_cast<int>(_codec_context->bit_rate / 2);
	_codec_context->sample_aspect_ratio = (AVRational){1, 1};
	_codec_context->ticks_per_frame = 2;
	_codec_context->time_base = ::av_inv_q(::av_mul_q(_codec_context->framerate, (AVRational){_codec_context->ticks_per_frame, 1}));
	_codec_context->gop_size = _codec_context->framerate.num / _codec_context->framerate.den;
	_codec_context->max_b_frames = 0;
	_codec_context->pix_fmt = (AVPixelFormat)GetPixelFormat();
	_codec_context->width = _encoder_context->GetVideoWidth();
	_codec_context->height = _encoder_context->GetVideoHeight();

	// Preset
	if (_encoder_context->GetPreset() == "slower")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p7", 0);
	}
	else if (_encoder_context->GetPreset() == "slow")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p6", 0);
	}
	else if (_encoder_context->GetPreset() == "medium")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p5", 0);
	}
	else if (_encoder_context->GetPreset() == "fast")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p4", 0);
	}
	else if (_encoder_context->GetPreset() == "faster")
	{
		::av_opt_set(_codec_context->priv_data, "preset", "p3", 0);
	}
	else
	{
		// Default
		::av_opt_set(_codec_context->priv_data, "preset", "p7", 0);
	}
	
	::av_opt_set(_codec_context->priv_data, "tune", "ull", 0);
	::av_opt_set(_codec_context->priv_data, "rc", "cbr", 0);

	return true;
}

// Notes.
// - B-frame must be disabled. because, WEBRTC does not support B-Frame.
bool EncoderHEVCxNV::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();

	AVCodec *codec = ::avcodec_find_encoder_by_name("hevc_nvenc");
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
		logte("Could not open codec: %s (%d)", codec->name, codec->id);
		return false;
	}

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderHEVCxNV::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("Enc%sNV", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start encoder thread.");
		_kill_flag = true;

		return false;
	}

	return true;
}

void EncoderHEVCxNV::CodecThread()
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
		if(!av_frame)
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
				auto media_packet = ffmpeg::Conv::ToMediaPacket(_packet, cmn::MediaType::Video, cmn::BitstreamFormat::H265_ANNEXB, cmn::PacketType::NALU);
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
