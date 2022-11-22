//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include <base/info/media_track.h>
#include "fmp4_storage.h"
#include "fmp4_private.h"

namespace bmff
{
	FMP4Storage::FMP4Storage(const std::shared_ptr<FMp4StorageObserver> &observer, const std::shared_ptr<const MediaTrack> &track, const FMP4Storage::Config &config, const ov::String &log_tag)
	{
		_config = config;
		_track = track;
		_observer = observer;

		// Keep 2x more even if pushed out of chunklist
		_config.max_segments *= 2;
		
		_target_segment_duration_ms = static_cast<int64_t>(_config.segment_duration_ms);

		_log_tag = log_tag;
	}

	std::shared_ptr<ov::Data> FMP4Storage::GetInitializationSection() const
	{
		return _initialization_section;
	}

	std::shared_ptr<FMP4Segment> FMP4Storage::GetMediaSegment(uint32_t segment_number) const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_lock);
		auto index = segment_number - _number_of_deleted_segments;

		if (index >= _segments.size())
		{
			return nullptr;
		}

		return _segments[index];
	}

	std::shared_ptr<FMP4Segment> FMP4Storage::GetLastSegment() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_lock);
		if (_segments.empty())
		{
			return nullptr;
		}

		return _segments.back();
	}

	std::shared_ptr<FMP4Chunk> FMP4Storage::GetMediaChunk(uint32_t segment_number, uint32_t chunk_number) const
	{
		// Get Media Segement
		auto segment = GetMediaSegment(segment_number);
		if (segment == nullptr)
		{
			return nullptr;
		}

		// last chunk number + 1 of completed segment is the first chunk of the next segment
		if (segment->IsCompleted() && segment->GetLastChunkNumber() + 1 == chunk_number)
		{
			segment = GetMediaSegment(segment_number + 1);
			if (segment == nullptr)
			{
				return nullptr;
			}

			chunk_number = 0;
		}

		// Get Media Chunk
		auto chunk = segment->GetChunk(chunk_number);
		if (chunk == nullptr)
		{
			return nullptr;
		}

		return chunk;
	}

	std::tuple<int64_t, int64_t> FMP4Storage::GetLastChunkNumber() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_lock);
		if (_segments.empty())
		{
			return { -1, -1 };
		}

		auto last_segment = _segments.back();

		return { last_segment->GetNumber(), last_segment->GetLastChunkNumber() };
	}

	int64_t FMP4Storage::GetLastSegmentNumber() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_lock);
		return _last_segment_number;
	}

	uint64_t FMP4Storage::GetMaxChunkDurationMs() const
	{
		return _max_chunk_duration_ms;
	}

	uint64_t FMP4Storage::GetMinChunkDurationMs() const
	{
		return _min_chunk_duration_ms;
	}

	bool FMP4Storage::StoreInitializationSection(const std::shared_ptr<ov::Data> &section)
	{
		_initialization_section = section;
		if (_observer != nullptr)
		{
			_observer->OnFMp4StorageInitialized(_track->GetId());
		}
		return true;
	}

	int64_t FMP4Storage::GetTargetSegmentDuration() const
	{
		return _target_segment_duration_ms;
	}

	bool FMP4Storage::AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, int64_t start_timestamp, double duration_ms, bool independent, bool last_chunk)
	{
		auto segment = GetLastSegment();

		if (segment == nullptr || segment->IsCompleted())
		{
			// Notify observer
			if (segment != nullptr && _observer != nullptr)
			{
				_observer->OnMediaSegmentUpdated(_track->GetId(), segment->GetNumber());
			}

			// Create new segment
			segment = std::make_shared<FMP4Segment>(GetLastSegmentNumber() + 1, _config.segment_duration_ms);
			{
				std::lock_guard<std::shared_mutex> lock(_segments_lock);
				_segments.push_back(segment);
				_last_segment_number = segment->GetNumber();

				// Delete old segments
				if (_segments.size() > _config.max_segments)
				{
					_number_of_deleted_segments++;
					_segments.pop_front();
				}
			}
		}

		if (segment->AppendChunkData(chunk, start_timestamp, duration_ms, independent) == false)
		{
			return false;
		}

		// Complete Segment if segment duration is over and new chunk data is independent(new segment should be started with independent chunk)
		if (last_chunk == true)
		{
			segment->SetCompleted();

			logtd("Segment[%u] is created : track(%u), duration(%u) chunks(%u)", segment->GetNumber(), _track->GetId(),segment->GetDuration(), segment->GetChunkCount());

			// avg segment duration 
			_target_segment_duration_ms -= segment->GetDuration();
			_target_segment_duration_ms += _config.segment_duration_ms;

			if (_target_segment_duration_ms < static_cast<int64_t>(_config.segment_duration_ms / 2))
			{
				logtw("LLHLS stream(%s) is creating segments that are too large(%f ms). It seems that the keyframe interval is longer than the configured segment size.", _log_tag.CStr(), segment->GetDuration());
			}
		}

		_max_chunk_duration_ms = std::max(_max_chunk_duration_ms, duration_ms);
		_min_chunk_duration_ms = std::min(_min_chunk_duration_ms, duration_ms);

		// Notify observer
		if (_observer != nullptr)
		{
			_observer->OnMediaChunkUpdated(_track->GetId(), segment->GetNumber(), segment->GetLastChunkNumber());
		}

		return true;
	}
} // namespace bmff