//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_ffopus.h"

#include "../../transcoder_private.h"

EncoderFFOPUS::~EncoderFFOPUS()
{
}

bool EncoderFFOPUS::SetCodecParams()
{
	_codec_context->bit_rate = GetRefTrack()->GetBitrate();
	_codec_context->sample_fmt = (AVSampleFormat)GetSupportedFormat();
	_codec_context->sample_rate = GetRefTrack()->GetSampleRate();
	_codec_context->channel_layout = static_cast<uint64_t>(GetRefTrack()->GetChannel().GetLayout());
	_codec_context->channels = GetRefTrack()->GetChannel().GetCounts();
	
	// Support Frequency
	// 4000 - OPUS_BANDWIDTH_NARROWBAND (8kHz) (2~8 kbps)
	// 6000 - OPUS_BANDWIDTH_MEDIUMBAND (12kHz)
	// 8000 - OPUS_BANDWIDTH_WIDEBAND (16kHz) (9~12 kbps)
	// 12000 - OPUS_BANDWIDTH_SUPERWIDEBAND (24kHz)
	// 20000 - OPUS_BANDWIDTH_FULLBAND (48kHz) (13~2048 kbps)
	// _codec_context->cutoff = 20000;	 

	// Compression Level (0~10)
	// 0 Low quality and fast encoding (less CPU usage)
	// 10 - High quality, slow encoding (high CPU usage)
	_codec_context->compression_level = 10;

	::av_opt_set(_codec_context->priv_data, "application", "lowdelay", 0);
	::av_opt_set(_codec_context->priv_data, "frame_duration", "20.0", 0);
	::av_opt_set(_codec_context->priv_data, "packet_loss", "10", 0);
	::av_opt_set(_codec_context->priv_data, "vbr", "off", 0);

	return true;
}

bool EncoderFFOPUS::Configure(std::shared_ptr<MediaTrack> output_context)
{
	if (TranscodeEncoder::Configure(output_context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();
	const AVCodec *codec = ::avcodec_find_encoder(codec_id);
	if (codec == nullptr)
	{
		logte("Codec not found: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
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

	GetRefTrack()->SetAudioSamplesPerFrame(_codec_context->frame_size);

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderFFOPUS::CodecThread, this);
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

void EncoderFFOPUS::CodecThread()
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
		auto av_frame = ffmpeg::Conv::ToAVFrame(cmn::MediaType::Audio, media_frame);
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
			int ret = ::avcodec_receive_packet(_codec_context, _packet);
			if (ret == AVERROR(EAGAIN))
			{
				// Wait for more packet
				break;
			}
			else if (ret == AVERROR_EOF && ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
				auto media_packet = ffmpeg::Conv::ToMediaPacket(_packet, cmn::MediaType::Audio, cmn::BitstreamFormat::OPUS, cmn::PacketType::RAW);
				if (media_packet == nullptr)
				{
					logte("Could not allocate the media packet");
					break;
				}

				::av_packet_unref(_packet);

				// TODO : If the pts value are under zero, the dash packettizer does not work.
				if (media_packet->GetPts() < 0) {
					continue;
				}

				SendOutputBuffer(std::move(media_packet));
			}
		}
	}
}
