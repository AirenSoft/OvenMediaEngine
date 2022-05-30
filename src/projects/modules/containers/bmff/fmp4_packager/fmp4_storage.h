//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "fmp4_structure.h"

namespace bmff
{
	class FMp4StorageObserver : public ov::EnableSharedFromThis<FMp4StorageObserver>
	{
	public:
		virtual void OnFMp4StorageInitialized(const int32_t &track_id) = 0;
		virtual void OnMediaSegmentUpdated(const int32_t &track_id, const uint32_t &segment_number) = 0;
		virtual void OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number) = 0;
	};

	class FMP4Storage
	{
	public:
		struct Config
		{
			uint32_t max_segments = 10;
			uint32_t segment_duration_ms = 6000;
		};

		FMP4Storage(const std::shared_ptr<FMp4StorageObserver> &observer, const std::shared_ptr<const MediaTrack> &track, const Config &config);

		std::shared_ptr<ov::Data> GetInitializationSection() const;
		std::shared_ptr<FMP4Segment> GetMediaSegment(uint32_t segment_number) const;
		std::shared_ptr<FMP4Segment> GetLastSegment() const;
		std::shared_ptr<FMP4Chunk> GetMediaChunk(uint32_t segment_number, uint32_t chunk_number) const;

		std::tuple<int64_t, int64_t> GetLastChunkNumber() const;
		int64_t GetLastSegmentNumber() const;

		bool StoreInitializationSection(const std::shared_ptr<ov::Data> &section);
		bool AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, uint64_t start_timestamp, uint64_t duration_ms, bool independent);

		uint64_t GetMaxChunkDurationMs() const;
		uint64_t GetMinChunkDurationMs() const;

	private:
		Config	_config;
		std::shared_ptr<const MediaTrack> _track;

		std::shared_ptr<ov::Data> _initialization_section = nullptr;
		// 0 -> 1 -> 2 -> push_back(new segemnt)
		std::deque<std::shared_ptr<FMP4Segment>> _segments;
		mutable std::shared_mutex _segments_lock;

		size_t _number_of_deleted_segments = 0; // For indexing of _segments

		int64_t _last_segment_number = -1;

		uint64_t _max_chunk_duration_ms = 0;
		uint64_t _min_chunk_duration_ms = std::numeric_limits<uint64_t>::max();

		std::shared_ptr<FMp4StorageObserver> _observer;
	};
}