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
#include <modules/marker/marker_box.h>

namespace bmff
{
	class FMP4Chunk
	{
	public:
		FMP4Chunk(const std::shared_ptr<ov::Data> &data, uint64_t number, int64_t start_timestamp, double duration_ms, bool independent)
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

		int64_t GetStartTimestamp() const
		{
			return _start_timestamp;
		}

		double GetDuration() const
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
		int64_t _start_timestamp = 0;
		double _duration_ms = 0;
		bool _independent = false;
		std::shared_ptr<ov::Data> _data;
	};

	class FMP4Segment
	{
	public:
		FMP4Segment(uint64_t number, uint64_t target_duration)
		{
			_number = number;

			// For performance, reserve memory for 4Mbps * target_duration (sec)
			_data = std::make_shared<ov::Data>(((1000.0 * 1000.0 * 4.0)/8.0) * (static_cast<double>(target_duration) / 1000.0));
		}

		FMP4Segment(uint64_t number, double duration_ms, const std::shared_ptr<ov::Data> &data)
		{
			_number = number;
			_duration_ms = duration_ms;
			_data = data;

			SetCompleted();
		}

		void SetCompleted()
		{
			_is_completed = true;
		}

		bool IsCompleted() const
		{
			return _is_completed;
		}

		bool AppendChunkData(const std::shared_ptr<ov::Data> &chunk_data, int64_t start_timestamp, double duration_ms, bool independent)
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
		double GetDuration() const
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

		bool HasMarker() const
		{
			return _markers.empty() == false;
		}

		void SetMarkers(const std::vector<Marker> &markers)
		{
			_markers = markers;
		}

		void AddMarkers(const std::vector<Marker> &markers)
		{
			_markers.insert(_markers.end(), markers.begin(), markers.end());
		}

		const std::vector<Marker> &GetMarkers() const
		{
			return _markers;
		}

	private:
		bool _is_completed = false;

		int64_t _number = -1;

		int64_t _start_timestamp = 0;
		double _duration_ms = 0;

		std::deque<std::shared_ptr<FMP4Chunk>> _chunks;
		mutable std::shared_mutex _chunks_lock;

		int64_t _last_chunk_number = -1;

		// Segment Data
		std::shared_ptr<ov::Data> _data;

		std::vector<Marker> _markers;
	};
}