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

bool EncoderAAC::SetCodecParams()
{
	_codec_context->bit_rate = GetRefTrack()->GetBitrate();
	_codec_context->sample_fmt = (AVSampleFormat)GetSupportedFormat();
	_codec_context->sample_rate = GetRefTrack()->GetSampleRate();
	_codec_context->channel_layout = static_cast<uint64_t>(GetRefTrack()->GetChannel().GetLayout());
	_codec_context->channels = GetRefTrack()->GetChannel().GetCounts();
	_codec_context->initial_padding = 0;

	_bitstream_format = cmn::BitstreamFormat::AAC_ADTS;
	
	_packet_type = cmn::PacketType::RAW;

	return true;
}


bool EncoderAAC::InitCodec()
{
	auto codec_id = GetCodecID();
	const AVCodec *codec = ::avcodec_find_encoder(codec_id);
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

	GetRefTrack()->SetAudioSamplesPerFrame(_codec_context->frame_size);

	return true;
}

bool EncoderAAC::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&EncoderAAC::CodecThread, this);
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
