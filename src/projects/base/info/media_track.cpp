//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "media_track.h"

#include <base/ovlibrary/converter.h>
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MediaTrack"

using namespace cmn;

MediaTrack::MediaTrack()
	: _id(0),
	  _codec_id(MediaCodecId::None),
	  _media_type(MediaType::Unknown),
	  _bitrate(0),
	  _byass(false),
	  _start_frame_time(0),
	  _last_frame_time(0)
{
}

MediaTrack::MediaTrack(const MediaTrack &media_track)
{
	_id = media_track._id;
	_media_type = media_track._media_type;
	_codec_id = media_track._codec_id;

	// Video
	_framerate = media_track._framerate;
	_width = media_track._width;
	_height = media_track._height;

	// Audio
	_sample = media_track._sample;
	_channel_layout = media_track._channel_layout;

	_time_base = media_track._time_base;

	_bitrate = media_track._bitrate;

	_byass = media_track._byass;

	_start_frame_time = 0;
	_last_frame_time = 0;

	_codec_extradata = media_track._codec_extradata;
}

MediaTrack::~MediaTrack()
{
}

void MediaTrack::SetId(uint32_t id)
{
	_id = id;
}

uint32_t MediaTrack::GetId() const
{
	return _id;
}

// Track Name (used for variant playlist)
void MediaTrack::SetName(const ov::String &name)
{
	_name = name;
}

ov::String MediaTrack::GetName() const
{
	return _name;
}

void MediaTrack::SetMediaType(MediaType type)
{
	_media_type = type;
}

MediaType MediaTrack::GetMediaType() const
{
	return _media_type;
}

void MediaTrack::SetCodecId(MediaCodecId id)
{
	_codec_id = id;
}

MediaCodecId MediaTrack::GetCodecId() const
{
	return _codec_id;
}

void MediaTrack::SetOriginBitstream(cmn::BitstreamFormat format)
{
	_origin_bitstream_format = format;
}

cmn::BitstreamFormat MediaTrack::GetOriginBitstream() const
{
	return _origin_bitstream_format;
}

const Timebase &MediaTrack::GetTimeBase() const
{
	return _time_base;
}

void MediaTrack::SetTimeBase(int32_t num, int32_t den)
{
	_time_base.Set(num, den);
}

void MediaTrack::SetTimeBase(const cmn::Timebase &time_base)
{
	_time_base = time_base;
}

void MediaTrack::SetBitrate(int32_t bitrate)
{
	_bitrate = bitrate;
}

int32_t MediaTrack::GetBitrate() const
{
	return _bitrate;
}

void MediaTrack::SetStartFrameTime(int64_t time)
{
	_start_frame_time = time;
}

int64_t MediaTrack::GetStartFrameTime() const
{
	return _start_frame_time;
}

void MediaTrack::SetLastFrameTime(int64_t time)
{
	_last_frame_time = time;
}

int64_t MediaTrack::GetLastFrameTime() const
{
	return _last_frame_time;
}

void MediaTrack::SetBypass(bool flag)
{
	_byass = flag;
}

bool MediaTrack::IsBypass()
{
	return _byass;
}

void MediaTrack::SetCodecExtradata(const std::shared_ptr<ov::Data> &codec_extradata)
{
	_codec_extradata = codec_extradata;
}

const std::shared_ptr<ov::Data> &MediaTrack::GetCodecExtradata() const
{
	return _codec_extradata;
}

std::shared_ptr<ov::Data> &MediaTrack::GetCodecExtradata()
{
	return _codec_extradata;
}

ov::String MediaTrack::GetInfoString()
{
	ov::String out_str = "";

	switch (GetMediaType())
	{
		case MediaType::Video:
			out_str.AppendFormat(
				"Video Track #%d: "
				"Bypass(%s) "
				"Bitrate(%s) "
				"codec(%d, %s) "
				"resolution(%dx%d) "
				"framerate(%.2ffps) ",
				GetId(),
				IsBypass() ? "true" : "false",
				ov::Converter::BitToString(GetBitrate()).CStr(),
				GetCodecId(), ::StringFromMediaCodecId(GetCodecId()).CStr(),
				GetWidth(), GetHeight(),
				GetFrameRate());
			break;

		case MediaType::Audio:
			out_str.AppendFormat(
				"Audio Track #%d: "
				"Bypass(%s) "
				"Bitrate(%s) "
				"codec(%d, %s) "
				"samplerate(%s) "
				"format(%s, %d) "
				"channel(%s, %d) ",
				GetId(),
				IsBypass() ? "true" : "false",
				ov::Converter::BitToString(GetBitrate()).CStr(),
				GetCodecId(), ::StringFromMediaCodecId(GetCodecId()).CStr(),
				ov::Converter::ToSiString(GetSampleRate(), 1).CStr(),
				GetSample().GetName(), GetSample().GetSampleSize() * 8,
				GetChannel().GetName(), GetChannel().GetCounts());
			break;

		default:
			break;
	}

	out_str.AppendFormat("timebase(%s)", GetTimeBase().ToString().CStr());

	return out_str;
}

bool MediaTrack::IsValid()
{
	if(_is_valid == true)
	{
		return true;
	}

	switch (GetCodecId())
	{
		case MediaCodecId::H264: {
			if (_width > 0 &&
				_height > 0 &&
				_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0 &&
				_codec_extradata != nullptr)

			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::H265: {
			if (_width > 0 &&
				_height > 0 &&
				_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Vp8: {
			if (_width > 0 &&
				_height > 0 &&
				_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Vp9:
		case MediaCodecId::Flv: {
			if (_width > 0 &&
				_height > 0 &&
				_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Jpeg:
		case MediaCodecId::Png: {
			if (_width > 0 &&
				_height > 0 &&
				_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Aac: {
			if (_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0 &&
				_channel_layout.GetCounts() > 0 &&
				_channel_layout.GetLayout() > cmn::AudioChannel::Layout::LayoutUnknown &&
				_codec_extradata != nullptr)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Opus: {
			if (_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0 &&
				_channel_layout.GetCounts() > 0 &&
				_channel_layout.GetLayout() > cmn::AudioChannel::Layout::LayoutUnknown && 
				_sample.GetRate() == cmn::AudioSample::Rate::R48000)
			{
				_is_valid = true;
				return true;
			}
		}
		break;
		case MediaCodecId::Mp3: {
			if (_time_base.GetNum() > 0 &&
				_time_base.GetDen() > 0 &&
				_channel_layout.GetCounts() > 0 &&
				_channel_layout.GetLayout() > cmn::AudioChannel::Layout::LayoutUnknown)
			{
				_is_valid = true;
				return true;
			}
		}
		break;

		default:
			break;
	}

	return false;
}