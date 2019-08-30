//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "bit_writer.h"

#include <base/media_route/media_type.h>
#include <base/ovlibrary/ovlibrary.h>

#include <string.h>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#define PACKTYZER_DEFAULT_TIMESCALE (90000)  // 90MHz
#define AVC_NAL_START_PATTERN_SIZE (4)		 // 0x00000001
#define ADTS_HEADER_SIZE (7)

#pragma pack(push, 1)

enum class PacketizerStreamType : int32_t
{
	Common,  // Video + Audio
	VideoOnly,
	AudioOnly,
};

enum class PacketizerType : int32_t
{
	Dash,
	Hls,
};

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
	Ts,  // Video + Audio
	Mp4Video,
	Mp4Audio,
};

struct SegmentData
{
public:
	SegmentData(
		common::MediaType media_type,
		int sequence_number,
		ov::String file_name,
		int64_t timestamp,
		uint64_t duration,
		std::shared_ptr<ov::Data> &data)
		: media_type(media_type),
		  sequence_number(sequence_number),
		  file_name(file_name),
		  create_time(::time(nullptr)),
		  duration(duration),
		  timestamp(timestamp),
		  data(data)
	{
	}

public:
	common::MediaType media_type = common::MediaType ::Unknown;
	int sequence_number = 0;
	ov::String file_name;
	time_t create_time = 0;
	uint64_t duration = 0ULL;
	int64_t timestamp = 0LL;
	std::shared_ptr<ov::Data> data;
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
						int64_t timestamp,
						uint64_t duration,
						uint64_t time_offset,
						const common::Timebase &timebase,
						std::shared_ptr<ov::Data> &data)
		: type(type),
		  timestamp(timestamp),
		  duration(duration),
		  time_offset(time_offset),
		  timebase(timebase),
		  data(data)
	{
	}

	PacketizerFrameData(PacketizerFrameType type,
						int64_t timestamp,
						uint64_t duration,
						uint64_t time_offset,
						const common::Timebase &timebase)
		: type(type),
		  timestamp(timestamp),
		  duration(duration),
		  time_offset(time_offset),
		  timebase(timebase),
		  data(std::make_shared<ov::Data>())
	{
	}

	PacketizerFrameType type = PacketizerFrameType::Unknown;
	int64_t timestamp = 0LL;
	uint64_t duration = 0ULL;
	uint64_t time_offset = 0ULL;
	common::Timebase timebase;
	std::shared_ptr<ov::Data> data;
};

#pragma pack(pop)
