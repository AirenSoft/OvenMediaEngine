//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <string.h>

#include <deque>
#include <map>
#include <mutex>
#include <vector>

#define PACKTYZER_DEFAULT_TIMESCALE (90000)	 // 90MHz
#define AVC_NAL_START_PATTERN_SIZE (4)		 // 0x00000001
#define ADTS_HEADER_SIZE (7)

#pragma pack(push, 1)

enum class PlayListType : int32_t
{
	M3u8,
	Mpd,
};

enum class SegmentType : int32_t
{
	MpegTs,
	M4S,
};

enum class SegmentDataType : int32_t
{
	None,
	Both,  // Audio + Video
	Video,
	Audio,
};

struct SegmentItem
{
public:
	SegmentItem(
		SegmentDataType type,
		int sequence_number,
		ov::String file_name,
		int64_t timestamp,
		int64_t timestamp_in_ms,
		int64_t duration,
		int64_t duration_in_ms,
		const std::shared_ptr<const ov::Data> &data)
		: creation_time(::time(nullptr)),
		  type(type),
		  sequence_number(sequence_number),
		  file_name(file_name),
		  timestamp(timestamp),
		  timestamp_in_ms(timestamp_in_ms),
		  duration(duration),
		  duration_in_ms(duration_in_ms),
		  data(data)
	{
	}

public:
	time_t creation_time = 0;

	SegmentDataType type = SegmentDataType::None;
	int sequence_number = 0;
	ov::String file_name;
	int64_t timestamp = 0L;
	int64_t timestamp_in_ms = 0L;
	int64_t duration = 0L;
	int64_t duration_in_ms = 0L;
	std::shared_ptr<const ov::Data> data;
};

enum class PacketizerFrameType
{
	Unknown = 'U',
	VideoKeyFrame = 'I',  // Key
	VideoPFrame = 'P',
	VideoBFrame = 'B',
	AudioFrame = 'A',
};

struct PacketizerFrameData
{
	PacketizerFrameData(PacketizerFrameType type,
						int64_t pts,
						int64_t dts,
						uint64_t duration,
						const cmn::Timebase &timebase,
						std::shared_ptr<ov::Data> &data)
		: type(type),
		  pts(pts),
		  dts(dts),
		  duration(duration),
		  timebase(timebase),
		  data(data)
	{
	}

	PacketizerFrameData(PacketizerFrameType type,
						int64_t pts,
						int64_t dts,
						uint64_t duration,
						const cmn::Timebase &timebase)
		: type(type),
		  pts(pts),
		  dts(dts),
		  duration(duration),
		  timebase(timebase),
		  data(std::make_shared<ov::Data>())
	{
	}

	PacketizerFrameType type = PacketizerFrameType::Unknown;
	int64_t pts = 0LL;
	int64_t dts = 0LL;
	uint64_t duration = 0ULL;
	cmn::Timebase timebase;
	std::shared_ptr<ov::Data> data;
};

#pragma pack(pop)
