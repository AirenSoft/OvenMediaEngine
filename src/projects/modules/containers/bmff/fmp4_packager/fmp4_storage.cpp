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
	FMP4Storage::FMP4Storage(const std::shared_ptr<FMp4StorageObserver> &observer, const std::shared_ptr<const MediaTrack> &track, const FMP4Storage::Config &config)
	{
		_config = config;
		_track = track;
		_observer = observer;
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

		// Get Media Chunk
		auto chunk = segment->GetChunk(chunk_number);
		if (chunk == nullptr)
		{
			return nullptr;
		}

		return chunk;
	}

	int64_t FMP4Storage::GetLastChunkNumber() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_lock);
		if (_segments.empty())
		{
			return -1;
		}

		return _segments.back()->GetLastChunkNumber();
	}

	int64_t FMP4Storage::GetLastSegmentNumber() const
	{
		return _last_segment_number;
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

	bool FMP4Storage::AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, uint64_t start_timestamp, uint32_t duration_ms, bool independent)
	{
		auto segment = GetLastSegment();

		// Complete Segment if segment duration is over and new chunk data is independent(new segment should be started with independent chunk)
		if (segment != nullptr && segment->GetDuration() + duration_ms > _config.segment_duration_ms && independent == true)
		{
			segment->SetCompleted();

			// Notify observer
			if (_observer != nullptr)
			{
				_observer->OnMediaSegmentUpdated(_track->GetId(), segment->GetNumber());
			}
			
			logtd("Segment[%u] is created : track(%u), duration(%u) chunks(%u)", segment->GetNumber(), _track->GetId(),segment->GetDuration(), segment->GetChunkCount());

#if DEBUG
			static bool dump = ov::Converter::ToBool(std::getenv("OME_DUMP_LLHLS"));
			if (dump)
			{
				auto file_name = ov::String::FormatString("%s_%u.mp4", StringFromMediaType(_track->GetMediaType()).CStr(), segment->GetNumber());

				auto dump_data = std::make_shared<ov::Data>(1000*1000);
				dump_data->Append(GetInitializationSection());
				dump_data->Append(segment->GetData());

				ov::DumpToFile(ov::PathManager::Combine(ov::PathManager::GetAppPath("dump/llhls"), file_name), dump_data);
			}
#endif
		}
		
		if (segment == nullptr || segment->IsCompleted())
		{
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

		// Notify observer
		if (_observer != nullptr)
		{
			_observer->OnMediaChunkUpdated(_track->GetId(), segment->GetNumber(), segment->GetLastChunkNumber());
		}

		return true;
	}
} // namespace bmff