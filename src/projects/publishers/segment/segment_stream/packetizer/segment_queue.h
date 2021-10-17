//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/media_track.h>
#include <base/ovlibrary/ovlibrary.h>

#include "packetizer_define.h"

// Return false if we need to stop iterating
using SegmentQueueIterator = std::function<bool(std::shared_ptr<const SegmentItem> segment_item)>;

class Packetizer;

class SegmentQueue
{
public:
	SegmentQueue(Packetizer *packetizer,
				 const ov::String &app_name, const ov::String &stream_name,
				 const std::shared_ptr<MediaTrack> &track,
				 int segment_count, int segment_save_count);

	std::shared_ptr<const SegmentItem> Append(std::shared_ptr<SegmentItem> segment_item);
	std::shared_ptr<const SegmentItem> Append(
		SegmentDataType data_type, int sequence_number,
		const ov::String &file_name,
		int64_t timestamp, int64_t timestamp_in_ms,
		int64_t duration, int64_t duration_in_ms,
		const std::shared_ptr<const ov::Data> &data);

	// Change the discontinuity flag of last segment
	void MarkAsDiscontinuity();

	void Iterate(SegmentQueueIterator segment_iterator);

	bool IsReadyForStreaming() const;

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const;

	size_t GetCount() const
	{
		std::lock_guard lock_guard(_segment_mutex);

		return _segments.size();
	}

protected:
	const char *GetPacketizerName() const;

	//--------------------------------------------------------------------
	// Those variables are used to debugging purpose
	//--------------------------------------------------------------------
	Packetizer *_packetizer = nullptr;

	ov::String _app_name;
	ov::String _stream_name;

	std::shared_ptr<const MediaTrack> _track = nullptr;
	//--------------------------------------------------------------------

	// The number of items to be included in the playlist
	int _segment_count = -1;
	// The maximum number of items to store to _segments
	int _segment_save_count = -1;

	mutable std::mutex _segment_mutex;

	// A variable that has as many segments as _segment_save_count
	std::deque<std::shared_ptr<SegmentItem>> _segments;

	// A map for fast search with the file name
	// It should always be synchronized with _segments.
	//
	// Key: file name
	std::unordered_map<ov::String, std::shared_ptr<SegmentItem>> _segments_map;
};
