//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace bmff
{
	class FMP4Chunk
	{
	public:
		FMP4Chunk(const std::shared_ptr<ov::Data> &data, uint64_t number, uint64_t duration_ms)
		{
			_data = data;
			_number = number;
			_duration_ms = duration_ms;
		}

		uint64_t GetNumber() const
		{
			return _number;
		}

		uint64_t GetDurationMs() const
		{
			return _duration_ms;
		}

		const std::shared_ptr<ov::Data> &GetData() const
		{
			return _data;
		}

	private:
		uint64_t _number = 0;
		uint64_t _duration_ms = 0;
		std::shared_ptr<ov::Data> _data;
	};

	class FMP4Segment
	{
	public:
		FMP4Segment(uint64_t number, uint64_t target_duration)
		{
			_number = number;

			// For performance, reserve memory for 4Mbps * target_duration
			_data = std::make_shared<ov::Data>(((1000 * 1000 * 4)/8) * target_duration);
		}

		void SetCompleted()
		{
			_is_completed = true;
		}

		bool IsCompleted() const
		{
			return _is_completed;
		}

		bool AppendChunkData(const std::shared_ptr<ov::Data> &chunk_data, uint64_t duration_ms)
		{
			if (_is_completed)
			{
				return false;
			}

			std::unique_lock<std::shared_mutex> lock(_chunks_lock);
			_chunks.emplace_back(std::make_shared<FMP4Chunk>(chunk_data, _chunks.size(), duration_ms));
			lock.unlock();
			
			// Append data
			_duration_ms += duration_ms;

			_data->Append(chunk_data);

			return true;
		}

		// Get Data
		std::shared_ptr<ov::Data> GetData() const
		{
			return _data;
		}

		// Get Number
		uint64_t GetNumber() const
		{
			return _number;
		}

		// Get Duration
		uint64_t GetDuration() const
		{
			return _duration_ms;
		}

		// Get Chunk Count
		uint64_t GetChunkCount() const
		{
			std::shared_lock<std::shared_mutex> lock(_chunks_lock);
			return _chunks.size();
		}

		// Get Chunk At
		std::shared_ptr<FMP4Chunk> GetChunk(uint64_t index) const
		{
			std::shared_lock<std::shared_mutex> lock(_chunks_lock);

			if (index >= _chunks.size())
			{
				return nullptr;
			}

			return _chunks[index];
		}

	private:
		bool _is_completed = false;

		uint64_t _number = 0;
		uint64_t _duration_ms = 0;

		std::deque<std::shared_ptr<FMP4Chunk>> _chunks;
		mutable std::shared_mutex _chunks_lock;

		// Segment Data
		std::shared_ptr<ov::Data> _data;
	};
}