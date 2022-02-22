//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_stream.h"

#include <base/publisher/publisher.h>
#include <config/items/items.h>

#include "packetizer/packetizer.h"
#include "segment_stream_private.h"

SegmentStream::SegmentStream(const std::shared_ptr<pub::Application> application, const info::Stream &info,
							 PacketizerFactory packetizer_factory, int segment_duration)
	: Stream(application, info)
	, _packetizer_factory(packetizer_factory)
	, _segment_duration(segment_duration)

{
	OV_ASSERT2(_packetizer_factory != nullptr);
}

bool SegmentStream::CheckCodec(cmn::MediaType type, cmn::MediaCodecId codec_id)
{
	switch (type)
	{
		case cmn::MediaType::Video:
			switch (codec_id)
			{
				case cmn::MediaCodecId::H264:
				case cmn::MediaCodecId::H265:
					return true;

				default:
					// Not supported codec
					return false;
			}
			break;

		case cmn::MediaType::Audio:
			switch (codec_id)
			{
				case cmn::MediaCodecId::Aac:
					return true;

				default:
					// Not supported codec
					break;
			}
			break;

		default:
			break;
	}

	return false;
}

bool SegmentStream::GetPlayList(ov::String &play_list)
{
	if (_packetizer != nullptr)
	{
		return _packetizer->GetPlayList(play_list);
	}

	return false;
}

std::shared_ptr<const SegmentItem> SegmentStream::GetSegmentData(const ov::String &file_name) const
{
	if (_packetizer == nullptr)
	{
		return nullptr;
	}

	return _packetizer->GetSegmentData(file_name);
}

bool SegmentStream::Start()
{
	if (GetState() != State::CREATED)
	{
		return false;
	}

	std::shared_ptr<MediaTrack> video_track = nullptr;
	std::shared_ptr<MediaTrack> audio_track = nullptr;

	for (auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		if (CheckCodec(track->GetMediaType(), track->GetCodecId()))
		{
			switch (track->GetMediaType())
			{
				case cmn::MediaType::Video:
					video_track = track;
					break;

				case cmn::MediaType::Audio:
					audio_track = track;
					break;

				default:
					// Not implemented for another media types
					break;
			}
		}
	}

	bool video_enabled = (video_track != nullptr);
	bool audio_enabled = (audio_track != nullptr);

	if ((video_enabled == false) && (audio_enabled == false))
	{
		auto application = GetApplication();

		logtw("Stream [%s/%s] was not created because there were no supported codecs",
			  application->GetName().CStr(), GetName().CStr());

		return false;
	}

	_video_track = video_track;
	_audio_track = audio_track;

	_packetizer = _packetizer_factory(GetApplicationName(), GetName().CStr(), _video_track, _audio_track);
	_packetizer->ResetPacketizer(GetMsid());

	return Stream::Start();
}

bool SegmentStream::Stop()
{
	return Stream::Stop();
}

bool SegmentStream::OnStreamUpdated(const std::shared_ptr<info::Stream> &info)
{
	if (Stream::OnStreamUpdated(info) == false)
	{
		return false;
	}

	_last_msid = info->GetMsid();

	if (_packetizer != nullptr)
	{
		_packetizer->ResetPacketizer(_last_msid);
	}

	return true;
}

void SegmentStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_video_track != nullptr)
	{
		if (_video_track->GetId() == static_cast<uint32_t>(media_packet->GetTrackId()))
		{
			if (_packetizer != nullptr)
			{
				_packetizer->AppendVideoPacket(media_packet);
			}
		}
		else
		{
			// Unsupported Codec
		}
	}
	else
	{
		// _video_track became nullptr due to unsupported codec, etc.
	}
}

void SegmentStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_audio_track != nullptr)
	{
		if (_audio_track->GetId() == static_cast<uint32_t>(media_packet->GetTrackId()))
		{
			if (_packetizer != nullptr)
			{
				_packetizer->AppendAudioPacket(media_packet);
			}
		}
		else
		{
			// Unsuppported Codec
		}
	}
	else
	{
		// _audio_track became nullptr due to unsupported codec, etc.
	}
}
