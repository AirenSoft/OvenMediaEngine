//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "llhls_playlist.h"
#include "llhls_private.h"

LLHlsPlaylist::LLHlsPlaylist(const std::shared_ptr<const MediaTrack> &track, uint32_t max_segments, uint32_t target_duration, float part_target_duration, const ov::String &map_uri)
{
	_track = track;
	_max_segments = max_segments;
	_target_duration = target_duration;
	_part_target_duration = part_target_duration;
	_map_uri = map_uri;
}

bool LLHlsPlaylist::AppendSegmentInfo(const SegmentInfo &info)
{
	if (info.GetSequence() < _last_segment_sequence)
	{
		return false;
	}

	std::shared_ptr<SegmentInfo> segment = GetSegmentInfo(info.GetSequence());
	if (segment == nullptr)
	{
		// Sequence must be sequential
		if (_last_segment_sequence + 1 != info.GetSequence())
		{
			return false;
		}

		// Create segment
		segment = std::make_shared<SegmentInfo>(info);
		_segments.push_back(segment);

		_last_segment_sequence += 1;
	}
	else
	{
		// Update segment
		segment->UpdateInfo(info.GetStartTime(), info.GetDuration(), info.GetSize(), info.GetUrl(), info.IsIndependent());
	}

	segment->SetCompleted();

	_content_updated = true;

	return true;
}

bool LLHlsPlaylist::AppendPartialSegmentInfo(uint32_t segment_sequence, const SegmentInfo &info)
{
	if (segment_sequence < _last_segment_sequence)
	{
		return false;
	}

	std::shared_ptr<SegmentInfo> segment = GetSegmentInfo(segment_sequence);
	if (segment == nullptr)
	{
		// Create segment
		segment = std::make_shared<SegmentInfo>(segment_sequence);
		_segments.push_back(segment);

		_last_segment_sequence += 1;
	}

	segment->InsertPartialSegmentInfo(std::make_shared<SegmentInfo>(info));

	_content_updated = true;

	return true;
}

int64_t LLHlsPlaylist::GetSegmentIndex(uint32_t segment_sequence) const
{
	return segment_sequence - _deleted_segments;
}

std::shared_ptr<LLHlsPlaylist::SegmentInfo> LLHlsPlaylist::GetSegmentInfo(uint32_t segment_sequence) const
{
	auto index = GetSegmentIndex(segment_sequence);
	if (index < 0)
	{
		// This cannot be happened
		OV_ASSERT2(false);
		return nullptr;
	}

	if (_segments.size() < static_cast<size_t>(index + 1))
	{
		return nullptr;
	}

	return _segments[index];
}

ov::String LLHlsPlaylist::ToString(bool skip/*=false*/)
{
	// Create playlist
	if (_content_updated == true)
	{
		CreatePlaylist();
	}

	if (skip == false)
	{
		return _playlist_cache;
	}
	else
	{
		return _playlist_skipped_cache;
	}

	return "";
}

bool LLHlsPlaylist::CreatePlaylist()
{
	if (_segments.size() == 0)
	{
		return false;
	}

	// In OME, CAN-SKIP-UNTIL works if the playlist has at least 10 segments
	float can_skip_until = 0;
	uint32_t skipped_segment = 0;
	if (_segments.size() >= 10)
	{
		can_skip_until = _target_duration * (_segments.size()/3);
		skipped_segment = (_segments.size()/3);
	}

	// version 0 : EXT-X-VERSION:6 - Contains EXT-X-MAP tag
	// version 1 : EXT-X-VERSION:9 - Contains EXT-X-SKIP tag
	for (int version=0; version<2; version++)
	{
		ov::String playlist;

		playlist.AppendFormat("#EXTM3U\n");

		// Note that in protocol version 6, the semantics of the EXT-
		// X-TARGETDURATION tag changed slightly.  In protocol version 5 and
		// earlier it indicated the maximum segment duration; in protocol
		// version 6 and later it indicates the the maximum segment duration
		// rounded to the nearest integer number of seconds.
		playlist.AppendFormat("#EXT-X-TARGETDURATION:%u\n", static_cast<uint32_t>(std::round(_target_duration)));

		// X-SERVER-CONTROL
		playlist.AppendFormat("#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES,PART-HOLD-BACK=%.1f",	_part_target_duration * 3);
		if (can_skip_until > 0)
		{
			playlist.AppendFormat(",CAN-SKIP-UNTIL=%.1f\n", can_skip_until);
		}
		else
		{
			playlist.AppendFormat("\n");
		}
		playlist.AppendFormat("#EXT-X-VERSION:%d\n", version == 0 ? 6 : 9);
		playlist.AppendFormat("#EXT-X-PART-INF:PART-TARGET=%f\n", _part_target_duration);
		playlist.AppendFormat("#EXT-X-MEDIA-SEQUENCE:%u\n", _segments[0]->GetSequence());
		playlist.AppendFormat("#EXT-X-MAP:URI=\"%s\"\n", _map_uri.CStr());

		uint32_t skip_count = 0;

		for (auto &segment : _segments)
		{
			if (version == 1 && skip_count < skipped_segment)
			{
				if (skip_count == 0) 
				{
					playlist.AppendFormat("#EXT-X-SKIP:SKIPPED-SEGMENTS=%u\n", skip_count);
				}
				skip_count += 1;
				continue;
			}

			std::chrono::system_clock::time_point tp{std::chrono::milliseconds{segment->GetStartTime()}};
			playlist.AppendFormat("#EXT-X-PROGRAM-DATE-TIME:%s\n", ov::Converter::ToISO8601String(tp).CStr());

			// Output partial segments info
			// Only output partial segments for the last 4 segments.
			if (int(segment->GetSequence()) > int(_segments.back()->GetSequence()) - 4)
			{
				for (auto &partial_segment : segment->GetPartialSegments())
				{
					playlist.AppendFormat("#EXT-X-PART:DURATION=%.3f,URI=\"%s\"",
										partial_segment->GetDuration(), partial_segment->GetUrl().CStr());
					if (_track->GetMediaType() == cmn::MediaType::Video && partial_segment->IsIndependent() == true)
					{
						playlist.AppendFormat(",INDEPENDENT=YES");
					}
					playlist.Append("\n");

					// If it is the last one, output PRELOAD-HINT
					if (segment == _segments.back() &&
						partial_segment == segment->GetPartialSegments().back())
					{
						playlist.AppendFormat("#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"%s\"\n", partial_segment->GetNextUrl().CStr());
					}
				}
			}

			if (segment->IsCompleted())
			{
				playlist.AppendFormat("#EXTINF:%.3f,\n", segment->GetDuration());
				playlist.AppendFormat("%s\n", segment->GetUrl().CStr());
			}
		}

		if (version == 0)
		{
			_playlist_cache = playlist;
		}
		else
		{
			_playlist_skipped_cache = playlist;
		}
	}

	return true;
}