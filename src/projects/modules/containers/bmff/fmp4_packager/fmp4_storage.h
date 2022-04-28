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
	class FMP4Storage
	{
	public:
		struct Config
		{
			uint32_t max_segments = 15;
			uint32_t segment_duration_ms = 5000;
		};

		FMP4Storage(const std::shared_ptr<const MediaTrack> &track, const Config &config);

		std::shared_ptr<ov::Data> GetInitializationSection() const;
		std::shared_ptr<FMP4Segment> GetMediaSegment(uint32_t segment_number) const;
		std::shared_ptr<FMP4Segment> GetLastSegment() const;
		std::shared_ptr<FMP4Chunk> GetMediaChunk(uint32_t segment_number, uint32_t chunk_number) const;

		uint32_t GetLastChunkNumber() const;
		uint32_t GetLastSegmentNumber() const;

		bool StoreInitializationSection(const std::shared_ptr<ov::Data> &section);
		bool AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, uint32_t duration_ms);

	private:
		Config	_config;
		std::shared_ptr<const MediaTrack> _track;

		std::shared_ptr<ov::Data> _initialization_section = nullptr;
		// 0 -> 1 -> 2 -> push_back(new segemnt)
		std::deque<std::shared_ptr<FMP4Segment>> _segments;
		mutable std::shared_mutex _segments_lock;

		size_t _number_of_deleted_segments = 0; // For indexing of _segments
	};
}