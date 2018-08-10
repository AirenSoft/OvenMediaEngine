#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <tuple>
#include <thread>
#include <condition_variable>

#include "ovlibrary/ovlibrary.h"

#include "media_route/media_type.h"

enum class FrameType : int8_t
{
	EmptyFrame,
	AudioFrameKey,
	AudioFrameDelta,
	AudioFrameSpeech,
	AudioFrameCN, // Comfort Noise, https://tools.ietf.org/html/rfc3389
	VideoFrameKey,
	VideoFrameDelta,
};

struct FragmentationHeader
{
public:
	~FragmentationHeader()
	{
		OV_SAFE_DELETE(fragmentation_offset);
		OV_SAFE_DELETE(fragmentation_length);
		OV_SAFE_DELETE(fragmentation_time_diff);
		OV_SAFE_DELETE(fragmentation_pl_type);
	}

	// Number of fragmentations
	uint16_t fragmentation_vector_size = 0;
	// Offset of pointer to data for each
	size_t *fragmentation_offset = nullptr;

	// fragmentation
	// Data size for each fragmentation
	size_t *fragmentation_length = nullptr;
	// Timestamp difference relative "now" for each fragmentation
	uint16_t *fragmentation_time_diff = nullptr;
	// Payload type of each fragmentation
	uint8_t *fragmentation_pl_type = nullptr;
};

struct EncodedFrame
{
public:
	EncodedFrame(uint8_t *buffer, size_t length, size_t size)
		: buffer(buffer), length(length), size(size)
	{
	}

	int32_t encoded_width = 0;
	int32_t encoded_height = 0;

	int32_t time_stamp = 0;

	FrameType frame_type = FrameType::VideoFrameDelta;
	uint8_t *buffer = nullptr;
	size_t length = 0;
	size_t size = 0;
	bool complete_frame = false;
};

struct CodecSpecificInfoGeneric
{
	uint8_t simulcast_idx = 0;      // TODO: 향후 확장 구현에서 사용
};

static const int32_t kCodecTypeAudio = 0x10000000;
static const int32_t kCodecTypeVideo = 0x20000000;

enum class CodecType : int32_t
{
	// Audio Codecs
		Opus = kCodecTypeAudio + 1,

	// Video Codecs
		Vp8 = kCodecTypeVideo + 1,
	Vp9,
	H264,
	I420,
	Red,
	Ulpfec,
	Flexfec,
	Generic,
	Stereo,

	Unknown
};

struct CodecSpecificInfoVp8
{
	int16_t picture_id = 0;         // Negative value to skip pictureId.
	bool non_reference = false;
	uint8_t simulcast_idx = 0;
	uint8_t temporal_idx = 0;
	bool layer_sync = false;
	int tl0_pic_idx = 0;            // Negative value to skip tl0PicIdx.
	int8_t key_idx = 0;             // Negative value to skip keyIdx.
};

struct CodecSpecificInfoOpus
{
	int sample_rate_hz;
	size_t num_channels;
	int default_bitrate_bps;
	int min_bitrate_bps;
	int max_bitrate_bps;
};

union CodecSpecificInfoUnion
{
	CodecSpecificInfoGeneric generic;

	CodecSpecificInfoVp8 vp8;

	// In the future
	// RTPVideoHeaderVP9 vp9;

	CodecSpecificInfoOpus opus;
};

struct CodecSpecificInfo
{
	CodecType codec_type = CodecType::Unknown;
	const char *codec_name = nullptr;
	CodecSpecificInfoUnion codec_specific = { 0 };
};
