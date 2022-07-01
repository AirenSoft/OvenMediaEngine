//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "llhls_chunklist.h"
#include "llhls_private.h"

#include <base/ovlibrary/zip.h>

LLHlsChunklist::LLHlsChunklist(const ov::String &url, const std::shared_ptr<const MediaTrack> &track, uint32_t max_segments, uint32_t target_duration, float part_target_duration, const ov::String &map_uri)
{
	_url = url;
	_track = track;
	_max_segments = max_segments;
	_target_duration = target_duration;
	_max_part_duration = 0;
	_part_target_duration = part_target_duration;
	_map_uri = map_uri;
}

const ov::String& LLHlsChunklist::GetUrl() const
{
	return _url;
}

void LLHlsChunklist::SetPartHoldBack(const float &part_hold_back)
{
	_part_hold_back = part_hold_back;
}

bool LLHlsChunklist::AppendSegmentInfo(const SegmentInfo &info)
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

		// Lock
		std::unique_lock<std::shared_mutex> lock(_segments_guard);
		// Create segment
		segment = std::make_shared<SegmentInfo>(info);
		_segments.push_back(segment);

		_last_segment_sequence += 1;

		if (_segments.size() > _max_segments)
		{
			_segments.pop_front();
		}
	}
	else
	{
		// Update segment
		segment->UpdateInfo(info.GetStartTime(), info.GetDuration(), info.GetSize(), info.GetUrl(), info.IsIndependent());
	}

	segment->SetCompleted();

	return true;
}

bool LLHlsChunklist::AppendPartialSegmentInfo(uint32_t segment_sequence, const SegmentInfo &info)
{
	if (segment_sequence < _last_segment_sequence)
	{
		return false;
	}

	std::shared_ptr<SegmentInfo> segment = GetSegmentInfo(segment_sequence);
	if (segment == nullptr)
	{
		// Lock
		std::unique_lock<std::shared_mutex> lock(_segments_guard);

		// Create segment
		segment = std::make_shared<SegmentInfo>(segment_sequence);
		_segments.push_back(segment);

		_last_segment_sequence = segment_sequence;

		if (_segments.size() > _max_segments)
		{
			_segments.pop_front();
			_deleted_segments += 1;
		}
	}

	_max_part_duration = std::max(_max_part_duration, info.GetDuration());

	segment->InsertPartialSegmentInfo(std::make_shared<SegmentInfo>(info));
	_last_partial_segment_sequence = info.GetSequence();

	return true;
}

int64_t LLHlsChunklist::GetSegmentIndex(uint32_t segment_sequence) const
{
	return segment_sequence - _deleted_segments;
}

std::shared_ptr<LLHlsChunklist::SegmentInfo> LLHlsChunklist::GetSegmentInfo(uint32_t segment_sequence) const
{
	// lock
	std::unique_lock<std::shared_mutex> lock(_segments_guard);

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

bool LLHlsChunklist::GetLastSequenceNumber(int64_t &msn, int64_t &psn) const
{
	msn = _last_segment_sequence;
	psn = _last_partial_segment_sequence;

	return true;
}

ov::String LLHlsChunklist::ToString(const ov::String &query_string, const std::map<int32_t, std::shared_ptr<LLHlsChunklist>> &renditions, bool skip, bool legacy) const
{
	if (_segments.size() == 0)
	{
		return "";
	}

	// TODO(Getroot) : Implement _HLS_skip=YES (skip = true)

	ov::String playlist(20480);

	playlist.AppendFormat("#EXTM3U\n");

	playlist.AppendFormat("#EXT-X-VERSION:%d\n", 6);
	// Note that in protocol version 6, the semantics of the EXT-
	// X-TARGETDURATION tag changed slightly.  In protocol version 5 and
	// earlier it indicated the maximum segment duration; in protocol
	// version 6 and later it indicates the the maximum segment duration
	// rounded to the nearest integer number of seconds.
	playlist.AppendFormat("#EXT-X-TARGETDURATION:%u\n", static_cast<uint32_t>(std::round(_target_duration)));

	// Low Latency Mode
	if (legacy == false)
	{
		// X-SERVER-CONTROL
		playlist.AppendFormat("#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES,PART-HOLD-BACK=%f\n", _part_hold_back);
		playlist.AppendFormat("#EXT-X-PART-INF:PART-TARGET=%f\n", _max_part_duration);
	}

	playlist.AppendFormat("#EXT-X-MEDIA-SEQUENCE:%u\n", _segments[0]->GetSequence());
	playlist.AppendFormat("#EXT-X-MAP:URI=\"%s", _map_uri.CStr());
	if (query_string.IsEmpty() == false)
	{
		playlist.AppendFormat("?%s", query_string.CStr());
	}
	playlist.AppendFormat("\"\n");

	std::shared_lock<std::shared_mutex> segment_lock(_segments_guard);
	for (auto &segment : _segments)
	{
		std::chrono::system_clock::time_point tp{std::chrono::milliseconds{segment->GetStartTime()}};
		playlist.AppendFormat("#EXT-X-PROGRAM-DATE-TIME:%s\n", ov::Converter::ToISO8601String(tp).CStr());

		// Low Latency Mode
		if (legacy == false)
		{
			// Output partial segments info
			// Only output partial segments for the last 4 segments.
			if (int(segment->GetSequence()) > int(_segments.back()->GetSequence()) - 3)
			{
				for (auto &partial_segment : segment->GetPartialSegments())
				{
					playlist.AppendFormat("#EXT-X-PART:DURATION=%.3f,URI=\"%s",
										partial_segment->GetDuration(), partial_segment->GetUrl().CStr());
					if (query_string.IsEmpty() == false)
					{
						playlist.AppendFormat("?%s", query_string.CStr());
					}
					playlist.AppendFormat("\"");
					if (_track->GetMediaType() == cmn::MediaType::Video && partial_segment->IsIndependent() == true)
					{
						playlist.AppendFormat(",INDEPENDENT=YES");
					}
					playlist.Append("\n");

					// If it is the last one, output PRELOAD-HINT
					if (segment == _segments.back() &&
						partial_segment == segment->GetPartialSegments().back())
					{
						playlist.AppendFormat("#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"%s", partial_segment->GetNextUrl().CStr());
						if (query_string.IsEmpty() == false)
						{
							playlist.AppendFormat("?%s", query_string.CStr());
						}
						playlist.AppendFormat("\"\n");
					}
				}
			}
		}

		if (segment->IsCompleted())
		{
			playlist.AppendFormat("#EXTINF:%.3f,\n", segment->GetDuration());
			playlist.AppendFormat("%s", segment->GetUrl().CStr());
			if (query_string.IsEmpty() == false)
			{
				playlist.AppendFormat("?%s", query_string.CStr());
			}
			playlist.Append("\n");
		}
	}
	segment_lock.unlock();

	// Output #EXT-X-RENDITION-REPORT
	for (const auto &[track_id, rendition] : renditions)
	{
		// Skip mine 
		if (track_id == static_cast<int32_t>(_track->GetId()))
		{
			continue;
		}

		playlist.AppendFormat("#EXT-X-RENDITION-REPORT:URI=\"%s", rendition->GetUrl().CStr());
		if (query_string.IsEmpty() == false)
		{
			playlist.AppendFormat("?%s", query_string.CStr());
		}
		playlist.AppendFormat("\"");

		// LAST-MSN, LAST-PART
		int64_t last_msn, last_part;
		rendition->GetLastSequenceNumber(last_msn, last_part);

		if (legacy == true && last_msn > 0)
		{
			// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.5.4
			// If the Rendition contains Partial Segments then this value 
			// is the Media Sequence Number of the last Partial Segment. 

			// In legacy, the completed msn is reported.
			last_msn -= 1;
		}

		playlist.AppendFormat(",LAST-MSN=%llu", last_msn);

		if (legacy == false)
		{
			playlist.AppendFormat(",LAST-PART=%llu", last_part);
		}
		
		playlist.AppendFormat("\n");
	}

	return playlist;
}

std::shared_ptr<const ov::Data> LLHlsChunklist::ToGzipData(const ov::String &query_string, const std::map<int32_t, std::shared_ptr<LLHlsChunklist>> &renditions, bool skip, bool legacy) const
{
	return ov::Zip::CompressGzip(ToString(query_string, renditions, skip, legacy).ToData(false));
}
