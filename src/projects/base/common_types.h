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

#define MAX_FRAG_COUNT 3

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
	// Number of fragmentations
	uint16_t fragmentation_vector_size = 0;
	// Offset of pointer to data for each
	size_t fragmentation_offset[MAX_FRAG_COUNT] {};

	// fragmentation
	// Data size for each fragmentation
	size_t fragmentation_length[MAX_FRAG_COUNT] {};
	// Timestamp difference relative "now" for each fragmentation
	uint16_t fragmentation_time_diff[MAX_FRAG_COUNT] {};
	// Payload type of each fragmentation
	uint8_t fragmentation_pl_type[MAX_FRAG_COUNT] {};

	void VerifyAndAllocateFragmentationHeader(const size_t size) {
		const auto size16 = static_cast<uint16_t>(size);
		if (fragmentation_vector_size < size16) {
			uint16_t oldVectorSize = fragmentation_vector_size;
			{
				// offset
				size_t* oldOffsets = fragmentation_offset;
				memset(fragmentation_offset + oldVectorSize, 0, sizeof(size_t) * (size16 - oldVectorSize));
				// copy old values
				memcpy(fragmentation_offset, oldOffsets, sizeof(size_t) * oldVectorSize);
			}
			// length
			{
				size_t* oldLengths = fragmentation_length;
				memset(fragmentation_length + oldVectorSize, 0, sizeof(size_t) * (size16 - oldVectorSize));
				memcpy(fragmentation_length, oldLengths, sizeof(size_t) * oldVectorSize);
			}
			// time diff
			{
				uint16_t* oldTimeDiffs = fragmentation_time_diff;
				memset(fragmentation_time_diff + oldVectorSize, 0, sizeof(uint16_t) * (size16 - oldVectorSize));
				memcpy(fragmentation_time_diff, oldTimeDiffs, sizeof(uint16_t) * oldVectorSize);
			}
			// Payload type
			{
				uint8_t* oldTimePlTypes = fragmentation_pl_type;
				memset(fragmentation_pl_type + oldVectorSize, 0, sizeof(uint8_t) * (size16 - oldVectorSize));
				memcpy(fragmentation_pl_type, oldTimePlTypes, sizeof(uint8_t) * oldVectorSize);
			}
			fragmentation_vector_size = size16;
		}
	}
};

struct EncodedFrame
{
public:
	EncodedFrame(std::shared_ptr<ov::Data> buffer, size_t length, size_t size)
		: _buffer(buffer), _length(length), _size(size)
	{
	}

	int32_t _encoded_width = 0;
	int32_t _encoded_height = 0;

	int64_t _time_stamp = 0;

	FrameType _frame_type = FrameType::VideoFrameDelta;
	std::shared_ptr<ov::Data> _buffer;
	size_t _length = 0;
	size_t _size = 0;
	bool _complete_frame = false;
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

enum class H264PacketizationMode {
	NonInterleaved = 0,  // Mode 1 - STAP-A, FU-A is allowed
	SingleNalUnit        // Mode 0 - only single NALU allowed
};

struct CodecSpecificInfoH264 {
	H264PacketizationMode packetization_mode;
	uint8_t simulcast_idx;
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

	CodecSpecificInfoH264 h264;
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
