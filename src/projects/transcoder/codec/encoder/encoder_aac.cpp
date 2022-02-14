//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_aac.h"

#include "../../transcoder_private.h"
#include "../codec_utilities.h"

EncoderAAC::~EncoderAAC()
{
}

bool EncoderAAC::SetCodecParams()
{
	_codec_context->bit_rate = _encoder_context->GetBitrate();
	_codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
	_codec_context->sample_rate = _encoder_context->GetAudioSampleRate();
	_codec_context->channel_layout = static_cast<uint64_t>(_encoder_context->GetAudioChannel().GetLayout());
	_codec_context->channels = _encoder_context->GetAudioChannel().GetCounts();

	return true;
}

bool EncoderAAC::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	auto codec_id = GetCodecID();
	AVCodec *codec = ::avcodec_find_encoder(codec_id);
	if (codec == nullptr)
	{
		logte("Codec not found: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	// create codec context
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

	// open codec
	if (::avcodec_open2(_codec_context, codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(codec_id), codec_id);
		return false;
	}

	_encoder_context->SetAudioSamplesPerFrame(_codec_context->frame_size);

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderAAC::CodecThread, this);
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

void EncoderAAC::CodecThread()
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
		auto av_frame = TranscoderUtilities::MediaFrameToAVFrame(cmn::MediaType::Audio, media_frame);
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
			int ret = ::avcodec_receive_packet(_codec_context, _packet);
			if (ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret == AVERROR_EOF && ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
				auto media_packet = TranscoderUtilities::AvPacketToMediaPacket(_packet, cmn::MediaType::Audio, cmn::BitstreamFormat::AAC_ADTS, cmn::PacketType::RAW);
				if (media_packet == nullptr)
				{
					logte("Could not allocate the media packet");
					break;
				}

				::av_packet_unref(_packet);

				// TODO : If the pts value are under zero, the dash packettizer does not work.
				if (media_packet->GetPts() < 0)
				{
					continue;
				}

				SendOutputBuffer(std::move(media_packet));
			}
		}
	}
}
