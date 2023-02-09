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

LLHlsChunklist::LLHlsChunklist(const ov::String &url, const std::shared_ptr<const MediaTrack> &track, uint32_t target_duration, double part_target_duration, const ov::String &map_uri)
{
	_url = url;
	_track = track;
	_target_duration = target_duration;
	_max_part_duration = 0;
	_part_target_duration = part_target_duration;
	_map_uri = map_uri;

	logtd("LLHLS Chunklist has been created. track(%s)", _track->GetVariantName().CStr());
}

LLHlsChunklist::~LLHlsChunklist()
{
	logtd("Chunklist has been deleted. %s", GetTrack()->GetVariantName().CStr());
}

// Set all renditions info for ABR
void LLHlsChunklist::SetRenditions(const std::map<int32_t, std::shared_ptr<LLHlsChunklist>> &renditions)
{
	_renditions = renditions;
}

void LLHlsChunklist::Release()
{
	_renditions.clear();
}

void LLHlsChunklist::SaveOldSegmentInfo(bool enable)
{
	_keep_old_segments = enable;

	if (_keep_old_segments == false)
	{
		_old_segments.clear();
		_old_segments.shrink_to_fit();
	}
}

const std::shared_ptr<const MediaTrack> &LLHlsChunklist::GetTrack() const
{
	return _track;
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
	logtd("AppendSegmentInfo[Track : %s/%s]: %s", _track->GetPublicName().CStr(), _track->GetVariantName().CStr(), info.ToString().CStr());

	if (info.GetSequence() < _last_segment_sequence)
	{
		logtc("The sequence number of the segment to be added is less than the last segment. segment(%lld) last(%lld)", info.GetSequence(), _last_segment_sequence.load());
		return false;
	}

	std::shared_ptr<SegmentInfo> segment = GetSegmentInfo(info.GetSequence());
	bool is_new_segment = false;
	if (segment == nullptr)
	{
		// Sequence must be sequential
		if (_last_segment_sequence + 1 != info.GetSequence())
		{
			logtc("Sequence is not sequential. last_sequence(%lld) current_sequence(%lld)", _last_segment_sequence.load(), info.GetSequence());
			return false;
		}

		// Lock
		std::unique_lock<std::shared_mutex> lock(_segments_guard);
		// Create segment
		segment = std::make_shared<SegmentInfo>(info);
		_segments.push_back(segment);
		is_new_segment = true;
	}
	else
	{
		// Update segment
		segment->UpdateInfo(info.GetStartTime(), info.GetDuration(), info.GetSize(), info.GetUrl(), info.IsIndependent());
	}

	segment->SetCompleted();

	UpdateCacheForDefaultChunklist();
	if (is_new_segment)
	{
		_last_segment_sequence = info.GetSequence();
	}

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
	}

	// part duration is calculated on first segment
	if (segment_sequence == 0)
	{
		_max_part_duration = std::max(_max_part_duration, info.GetDuration());
	}

	segment->InsertPartialSegmentInfo(std::make_shared<SegmentInfo>(info));
	
	UpdateCacheForDefaultChunklist();
	_last_partial_segment_sequence = info.GetSequence();

	return true;
}

bool LLHlsChunklist::RemoveSegmentInfo(uint32_t segment_sequence)
{
	std::unique_lock<std::shared_mutex> lock(_segments_guard);
	auto old_segment = _segments.front();
	SaveOldSegmentInfo(old_segment);

	_segments.pop_front();
	_deleted_segments += 1;
	return true;
}

void LLHlsChunklist::UpdateCacheForDefaultChunklist()
{
	ov::String chunklist = MakeChunklist("", false, false);
	{
		// lock 
		std::lock_guard<std::shared_mutex> lock(_cached_default_chunklist_guard);
		_cached_default_chunklist = chunklist;
	}

	{
		// lock 
		std::lock_guard<std::shared_mutex> lock(_cached_default_chunklist_gzip_guard);
		_cached_default_chunklist_gzip = ov::Zip::CompressGzip(chunklist.ToData(false));
	}
}

bool LLHlsChunklist::SaveOldSegmentInfo(std::shared_ptr<SegmentInfo> &segment_info)
{
	if (_keep_old_segments == false)
	{
		return true;
	}

	// no longer need partial segment info - for memory saving
	segment_info->ClearPartialSegments();

	logtd("Save old segment info: %d / %s", segment_info->GetSequence(), segment_info->GetUrl().CStr());
	_old_segments.push_back(segment_info);

	//TODD[CRITICAL](Getroot): _old_segments must be saved to file because memory should be exhausted

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
	std::shared_lock<std::shared_mutex> lock(_segments_guard);
	msn = _last_segment_sequence;
	psn = _last_partial_segment_sequence;
	return true;
}

ov::String LLHlsChunklist::MakeChunklist(const ov::String &query_string, bool skip, bool legacy, bool vod, uint32_t vod_start_segment_number) const
{
	if (_segments.size() == 0)
	{
		return "";
	}

	if (vod == true)
	{
		// VoD doesn't need Low-Latency HLS
		legacy = true;
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
		playlist.AppendFormat("#EXT-X-PART-INF:PART-TARGET=%lf\n", _max_part_duration);
	}

	playlist.AppendFormat("#EXT-X-MEDIA-SEQUENCE:%u\n", vod == false ? _segments[0]->GetSequence() : 0);
	playlist.AppendFormat("#EXT-X-MAP:URI=\"%s", _map_uri.CStr());
	if (query_string.IsEmpty() == false)
	{
		playlist.AppendFormat("?%s", query_string.CStr());
	}
	playlist.AppendFormat("\"\n");

	std::shared_lock<std::shared_mutex> segment_lock(_segments_guard);
	if (vod == true)
	{
		for (auto &segment : _old_segments)
		{
			if (segment->GetSequence() < vod_start_segment_number)
			{
				continue;
			}

			std::chrono::system_clock::time_point tp{std::chrono::milliseconds{segment->GetStartTime()}};
			playlist.AppendFormat("#EXT-X-PROGRAM-DATE-TIME:%s\n", ov::Converter::ToISO8601String(tp).CStr());
			playlist.AppendFormat("#EXTINF:%lf,\n", segment->GetDuration());
			playlist.AppendFormat("%s", segment->GetUrl().CStr());
			if (query_string.IsEmpty() == false)
			{
				playlist.AppendFormat("?%s", query_string.CStr());
			}
			playlist.Append("\n");
		}
	}

	for (auto &segment : _segments)
	{
		if (vod == true && segment->GetSequence() < vod_start_segment_number)
		{
			continue;
		}

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
					playlist.AppendFormat("#EXT-X-PART:DURATION=%lf,URI=\"%s",
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
			playlist.AppendFormat("#EXTINF:%lf,\n", segment->GetDuration());
			playlist.AppendFormat("%s", segment->GetUrl().CStr());
			if (query_string.IsEmpty() == false)
			{
				playlist.AppendFormat("?%s", query_string.CStr());
			}
			playlist.Append("\n");
		}
	}
	segment_lock.unlock();

	if (vod == false)
	{
		// Output #EXT-X-RENDITION-REPORT
		for (const auto &[track_id, rendition] : _renditions)
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
	}

	if (vod == true)
	{
		playlist.AppendFormat("#EXT-X-ENDLIST\n");
	}

	return playlist;
}

ov::String LLHlsChunklist::ToString(const ov::String &query_string, bool skip, bool legacy, bool vod, uint32_t vod_start_segment_number) const
{
	if (_segments.size() == 0)
	{
		return "";
	}

	if (query_string.IsEmpty() && skip == false && legacy == false && vod == false && vod_start_segment_number == 0 && !_cached_default_chunklist.IsEmpty())
	{
		// return cached chunklist for default chunklist
		std::shared_lock<std::shared_mutex> lock(_cached_default_chunklist_guard);
		return _cached_default_chunklist;
	}

	return MakeChunklist(query_string, skip, legacy, vod, vod_start_segment_number);
}

std::shared_ptr<const ov::Data> LLHlsChunklist::ToGzipData(const ov::String &query_string, bool skip, bool legacy) const
{
	if (query_string.IsEmpty() && skip == false && legacy == false && _cached_default_chunklist_gzip != nullptr)
	{
		std::shared_lock<std::shared_mutex> lock(_cached_default_chunklist_gzip_guard);
		return _cached_default_chunklist_gzip;
	}

	return ov::Zip::CompressGzip(ToString(query_string, skip, legacy).ToData(false));
}
