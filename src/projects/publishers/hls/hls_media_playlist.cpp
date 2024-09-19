//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_media_playlist.h"
#include "hls_private.h"

HlsMediaPlaylist::HlsMediaPlaylist(const ov::String &id, const ov::String &playlist_file_name, const HlsMediaPlaylistConfig &config)
	: _config(config)
	, _variant_name(id)
	, _playlist_file_name(playlist_file_name)
{
}

void HlsMediaPlaylist::AddMediaTrackInfo(const std::shared_ptr<MediaTrack> &track)
{
	_media_tracks.emplace(track->GetId(), track);

	if (_first_video_track == nullptr && track->GetMediaType() == cmn::MediaType::Video)
	{
		_first_video_track = track;
	}

	if (_first_audio_track == nullptr && track->GetMediaType() == cmn::MediaType::Audio)
	{
		_first_audio_track = track;
	}
}

void HlsMediaPlaylist::SetEndList()
{
	_end_list = true;
}

bool HlsMediaPlaylist::OnSegmentCreated(const std::shared_ptr<mpegts::Segment> &segment)
{
	std::lock_guard<std::shared_mutex> lock(_segments_mutex);

	logtd("HlsMediaPlaylist::OnSegmentCreated - number(%d) url(%s) duration_us(%llu)\n", segment->GetNumber(), segment->GetUrl().CStr(), segment->GetDurationUs());

	_segments.emplace(segment->GetNumber(), segment);

	return true;
}

bool HlsMediaPlaylist::OnSegmentDeleted(const std::shared_ptr<mpegts::Segment> &segment)
{
	std::lock_guard<std::shared_mutex> lock(_segments_mutex);

	logtd("HlsMediaPlaylist::OnSegmentDeleted - number(%d) url(%s) duration_us(%llu)\n", segment->GetNumber(), segment->GetUrl().CStr(), segment->GetDurationUs());

	auto it = _segments.find(segment->GetNumber());
	if (it == _segments.end())
	{
		logte("HlsMediaPlaylist::OnSegmentDeleted - Failed to find the segment number %d\n", segment->GetNumber());
		return false;
	}

	_segments.erase(it);

	return true;
}

ov::String HlsMediaPlaylist::ToString(bool rewind) const
{
	std::shared_lock<std::shared_mutex> lock(_segments_mutex);

	ov::String result = "#EXTM3U\n";

	result += ov::String::FormatString("#EXT-X-VERSION:%d\n", 3);
	if (rewind == true && _config.event_playlist_type == true)
	{
		result += ov::String::FormatString("#EXT-X-PLAYLIST-TYPE:EVENT\n");
	}
	result += ov::String::FormatString("#EXT-X-TARGETDURATION:%d\n", _config.target_duration);
	
	if (_segments.empty() == true)
	{
		return result;
	}

	std::shared_ptr<mpegts::Segment> first_segment = _segments.begin()->second;
	if (rewind == false)
	{
		size_t segment_size = _segments.size();
		size_t shift_count = segment_size > _config.segment_count ? _config.segment_count : segment_size - 1;
		uint64_t last_segment_number = _segments.rbegin()->second->GetNumber();
		
		auto it = _segments.find(last_segment_number - shift_count);
		if (it == _segments.end())
		{
			logte("Failed to find the first segment number %d\n", last_segment_number - shift_count);
			return result;
		}

		first_segment = it->second;
	}
	
	result += ov::String::FormatString("#EXT-X-MEDIA-SEQUENCE:%d\n", first_segment->GetNumber());

	for (auto it = _segments.find(first_segment->GetNumber()); it != _segments.end(); it ++)
	{
		const auto &segment = it->second;
		result += ov::String::FormatString("#EXTINF:%.3f,\n", static_cast<double>(segment->GetDurationUs()) / 1000000.0);
		result += ov::String::FormatString("%s\n", segment->GetUrl().CStr());
	}

	if (_end_list == true)
	{
		result += "#EXT-X-ENDLIST\n";
	}
	
	return result;
}

bool HlsMediaPlaylist::HasVideo() const
{
	return _first_video_track != nullptr;
}

bool HlsMediaPlaylist::HasAudio() const
{
	return _first_audio_track != nullptr;
}

uint32_t HlsMediaPlaylist::GetBitrates() const
{
	uint32_t bitrates = 0;
	for (const auto &track_it : _media_tracks)
	{
		const auto &track = track_it.second;
		bitrates += track->GetBitrateLastSecond();
	}

	return bitrates;
}

uint32_t HlsMediaPlaylist::GetAverageBitrate() const
{
	uint32_t bitrates = 0;
	for (const auto &track_it : _media_tracks)
	{
		const auto &track = track_it.second;

		// conf first, measure next
		bitrates += track->GetBitrate();
	}

	return bitrates;
}

bool HlsMediaPlaylist::GetResolution(uint32_t &width, uint32_t &height) const
{
	if (_first_video_track == nullptr)
	{
		return false;
	}

	width = _first_video_track->GetWidth();
	height = _first_video_track->GetHeight();

	return true;
}

ov::String HlsMediaPlaylist::GetResolutionString() const
{
	uint32_t width = 0;
	uint32_t height = 0;
	if (GetResolution(width, height) == false)
	{
		return "";
	}

	return ov::String::FormatString("%dx%d", width, height);
}

double HlsMediaPlaylist::GetFramerate() const
{
	if (_first_video_track == nullptr)
	{
		return 0.0;
	}

	return _first_video_track->GetFrameRate();
}

ov::String HlsMediaPlaylist::GetCodecsString() const
{
	ov::String result;
	
	if (_first_video_track != nullptr)
	{
		result += _first_video_track->GetCodecsParameter();
	}

	if (_first_audio_track != nullptr)
	{
		if (result.IsEmpty() == false)
		{
			result += ",";
		}

		result += _first_audio_track->GetCodecsParameter();
	}

	return result;
}