//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "bit_writer.h"

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

enum class PacketyzerStreamType : int32_t
{
	Common,  // Video + Audio
	VideoOnly,
	AudioOnly,
};

enum class PacketyzerType : int32_t
{
	Dash,
	Hls,
	Cmaf,
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
	SegmentData(int sequence_number,
				ov::String file_name,
				uint64_t timestamp,
				uint64_t duration,
				std::shared_ptr<ov::Data> &data)
		: sequence_number(sequence_number),
		  file_name(file_name),
		  create_time(::time(nullptr)),
		  duration(duration),
		  timestamp(timestamp),
		  data(data)
	{
	}

public:
	int sequence_number = 0;
	ov::String file_name;
	time_t create_time = 0;
	uint64_t duration = 0ULL;
	uint64_t timestamp = 0ULL;
	std::shared_ptr<ov::Data> data;
};

enum class PacketyzerFrameType
{
	Unknown = 'U',
	VideoKeyFrame = 'I',  // Key
	VideoPFrame = 'P',
	VideoBFrame = 'B',
	AudioFrame = 'A',
};

struct PacketyzerFrameData
{
public:
	PacketyzerFrameData(PacketyzerFrameType type,
						uint64_t timestamp,
						uint64_t time_offset,
						uint32_t time_scale,
						std::shared_ptr<ov::Data> &data)
		: type(type),
		  timestamp(timestamp),
		  time_offset(time_offset),
		  timescale(time_scale),
		  data(data)
	{
	}

	PacketyzerFrameData(PacketyzerFrameType type,
						uint64_t timestamp,
						uint64_t time_offset,
						uint32_t time_scale)
		: type(type),
		  timestamp(timestamp),
		  time_offset(time_offset),
		  timescale(time_scale),
		  data(std::make_shared<ov::Data>())
	{
	}

public:
	PacketyzerFrameType type = PacketyzerFrameType::Unknown;
	uint64_t timestamp = 0ULL;
	uint64_t time_offset = 0ULL;
	uint32_t timescale = 0U;
	std::shared_ptr<ov::Data> data;
};

enum class SegmentCodecType
{
	UnknownCodec,
	Vp8Codec,
	H264Codec,
	OpusCodec,
	AacCodec,
};

struct PacketyzerMediaInfo
{
	PacketyzerMediaInfo() = default;

	PacketyzerMediaInfo(SegmentCodecType video_codec_type, uint32_t video_width, uint32_t video_height, float video_framerate, uint32_t video_bitrate, uint32_t video_timescale,
						SegmentCodecType audio_codec_type, uint32_t audio_channels, uint32_t audio_samplerate, uint32_t audio_bitrate, uint32_t audio_timescale)
		: video_codec_type(video_codec_type),
		  video_width(video_width),
		  video_height(video_height),
		  video_framerate(video_framerate),
		  video_bitrate(video_bitrate),
		  video_timescale(video_timescale),

		  audio_codec_type(audio_codec_type),
		  audio_channels(audio_channels),
		  audio_samplerate(audio_samplerate),
		  audio_bitrate(audio_bitrate),
		  audio_timescale(audio_timescale)
	{
	}

	// 비디오 정보
	SegmentCodecType video_codec_type = SegmentCodecType::UnknownCodec;
	uint32_t video_width = 0;
	uint32_t video_height = 0;
	float video_framerate = 0.0f;
	uint32_t video_bitrate = 0;
	uint32_t video_timescale = 0;

	// 오디오 정보
	SegmentCodecType audio_codec_type = SegmentCodecType::UnknownCodec;
	uint32_t audio_channels = 0;
	uint32_t audio_samplerate = 0;
	uint32_t audio_bitrate = 0;
	uint32_t audio_timescale = 0;
};

#pragma pack(pop)
