//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_queue.h"

#include "../segment_stream_private.h"
#include "packetizer.h"

SegmentQueue::SegmentQueue(Packetizer *packetizer,
						   const ov::String &app_name, const ov::String &stream_name,
						   const std::shared_ptr<MediaTrack> &track,
						   int segment_count, int segment_save_count)
	: _packetizer(packetizer),

	  _app_name(app_name),
	  _stream_name(stream_name),

	  _track(track),

	  _segment_count(segment_count),
	  _segment_save_count(segment_save_count)
{
	OV_ASSERT2(_segment_count <= _segment_save_count);
	OV_ASSERT2(_segment_count > 0);
	OV_ASSERT2(_segment_save_count > 0);
}

const char *SegmentQueue::GetPacketizerName() const
{
	return (_packetizer != nullptr) ? _packetizer->GetPacketizerName() : "";
}

std::shared_ptr<const SegmentItem> SegmentQueue::Append(std::shared_ptr<SegmentItem> segment_item)
{
	std::lock_guard lock_guard(_segment_mutex);

	if (static_cast<int>(_segments.size()) == _segment_save_count)
	{
		const auto &front = _segments.front();

		_segments_map.erase(front->file_name);
		_segments.pop_front();
	}

	_segments.push_back(segment_item);
	_segments_map[segment_item->file_name] = segment_item;

	if (_track != nullptr)
	{
		[[maybe_unused]] auto time_scale = _track->GetTimeBase().GetTimescale();
		[[maybe_unused]] auto duration_in_ms = segment_item->duration_in_ms;

		logad("SegmentItem is added, type: %d, file: %s, pts: %" PRId64 "ms, duration: %" PRIu64 "ms, data size: %zubytes (scale: %llu/%.0f = %0.3f)",
			  segment_item->type,
			  segment_item->file_name.CStr(),
			  segment_item->timestamp_in_ms, duration_in_ms,
			  segment_item->data->GetLength(),

			  duration_in_ms,
			  time_scale,
			  (time_scale != 0.0) ? (double)duration_in_ms / time_scale : 0.0);
	}
	else
	{
		logad("SegmentItem is added for null track");
	}

	return segment_item;
}

std::shared_ptr<const SegmentItem> SegmentQueue::Append(
	SegmentDataType data_type, int sequence_number,
	const ov::String &file_name,
	int64_t timestamp, int64_t timestamp_in_ms,
	int64_t duration, int64_t duration_in_ms,
	const std::shared_ptr<const ov::Data> &data)
{
	return Append(std::make_shared<SegmentItem>(
		data_type, sequence_number,
		file_name,
		timestamp, timestamp_in_ms,
		duration, duration_in_ms,
		data));
}

void SegmentQueue::Iterate(SegmentQueueIterator segment_iterator)
{
	std::lock_guard lock_guard(_segment_mutex);

	auto size = static_cast<int>(_segments.size());

	int start_index = std::max(0, size - _segment_count);
	int end_index = std::min(start_index + _segment_count, size);

	auto begin_iterator = _segments.begin();

	auto start_iterator = begin_iterator + start_index;
	auto end_iterator = begin_iterator + end_index;

	for (auto iterator = start_iterator; iterator != end_iterator; iterator++)
	{
		if (segment_iterator(*iterator) == false)
		{
			break;
		}
	}
}

std::shared_ptr<const SegmentItem> SegmentQueue::GetSegmentData(const ov::String &file_name) const
{
	std::lock_guard lock_guard(_segment_mutex);

	auto segment = _segments_map.find(file_name);
	if (segment == _segments_map.end())
	{
		return nullptr;
	}

	return segment->second;
}

void SegmentQueue::MarkAsDiscontinuity()
{
	std::lock_guard lock_guard(_segment_mutex);

	if (_segments.empty() == false)
	{
		_segments.back()->discontinuity = true;
	}
}
