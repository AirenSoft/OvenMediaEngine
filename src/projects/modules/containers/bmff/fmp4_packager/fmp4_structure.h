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
		FMP4Chunk(const std::shared_ptr<ov::Data> &data, uint64_t number, uint64_t start_timestamp, uint64_t duration_ms, bool independent)
		{
			_data = data;
			_number = number;
			_duration_ms = duration_ms;
			_start_timestamp = start_timestamp;
			_independent = independent;
		}

		int64_t GetNumber() const
		{
			return _number;
		}

		uint64_t GetStartTimestamp() const
		{
			return _start_timestamp;
		}

		uint64_t GetDuration() const
		{
			return _duration_ms;
		}

		// Get Size
		uint64_t GetSize() const
		{
			return _data->GetLength();
		}

		bool IsIndependent() const
		{
			return _independent;
		}

		const std::shared_ptr<ov::Data> &GetData() const
		{
			return _data;
		}

	private:
		int64_t _number = -1;
		uint64_t _start_timestamp = 0;
		uint64_t _duration_ms = 0;
		bool _independent = false;
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

		bool AppendChunkData(const std::shared_ptr<ov::Data> &chunk_data, uint64_t start_timestamp, uint64_t duration_ms, bool independent)
		{
			if (_is_completed)
			{
				return false;
			}

			std::unique_lock<std::shared_mutex> lock(_chunks_lock);

			auto chunk_number = _chunks.size();
			if (chunk_number == 0)
			{
				_start_timestamp = start_timestamp;
			}

			_chunks.emplace_back(std::make_shared<FMP4Chunk>(chunk_data, chunk_number, start_timestamp, duration_ms, independent));
			_last_chunk_number = chunk_number;

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
		int64_t GetNumber() const
		{
			return _number;
		}

		// Get Start Timestamp
		uint64_t GetStartTimestamp() const
		{
			return _start_timestamp;
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

		size_t GetSize() const
		{
			return _data->GetLength();
		}

		// Get Last Chunk Number
		int64_t GetLastChunkNumber() const
		{
			return _last_chunk_number;
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

		int64_t _number = -1;

		uint64_t _start_timestamp = 0;
		uint64_t _duration_ms = 0;

		std::deque<std::shared_ptr<FMP4Chunk>> _chunks;
		mutable std::shared_mutex _chunks_lock;

		int64_t _last_chunk_number = -1;

		// Segment Data
		std::shared_ptr<ov::Data> _data;
	};
}