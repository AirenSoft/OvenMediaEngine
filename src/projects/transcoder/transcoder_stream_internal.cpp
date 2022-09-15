//==============================================================================
//
//  TranscoderStreamInternal
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_stream_internal.h"

#include "transcoder_private.h"

TranscoderStreamInternal::TranscoderStreamInternal()
{
}

TranscoderStreamInternal::~TranscoderStreamInternal()
{
}

ov::String TranscoderStreamInternal::GetIdentifiedForVideoProfile(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("T%d_PBYPASS", track_id);
	}

	return ov::String::FormatString("T%d_P%s-%d-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStreamInternal::GetIdentifiedForImageProfile(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	return ov::String::FormatString("T%d_P%s-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStreamInternal::GetIdentifiedForAudioProfile(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("T%d_PBYPASS", track_id);
	}

	return ov::String::FormatString("T%d_P%s-%d-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetSamplerate(),
									profile.GetChannel());
}

ov::String TranscoderStreamInternal::GetIdentifiedForDataProfile(const uint32_t track_id)
{
		return ov::String::FormatString("T%d_PBYPASS", track_id);
}


cmn::Timebase TranscoderStreamInternal::GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id)
{
	cmn::Timebase timebase(1, 1000);

	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::H265:
		case cmn::MediaCodecId::Vp8:
		case cmn::MediaCodecId::Vp9:
		case cmn::MediaCodecId::Flv:
		case cmn::MediaCodecId::Jpeg:
		case cmn::MediaCodecId::Png:
			timebase.SetNum(1);
			timebase.SetDen(90000);
			break;
		case cmn::MediaCodecId::Aac:
		case cmn::MediaCodecId::Mp3:
		case cmn::MediaCodecId::Opus:
			timebase.SetNum(1);
			timebase.SetDen(48000);
			break;
		default:
			break;
	}

	return timebase;
}

MediaTrackId TranscoderStreamInternal::NewTrackId()
{
	return _last_track_index++;
}