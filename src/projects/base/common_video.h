#pragma once

#include <cstddef>
#include <cstdint>

#include <base/ovlibrary/ovlibrary.h>

enum class VideoCodecType : int8_t
{
	Vp8,
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

enum class FrameType : int8_t
{
	EmptyFrame = 0,
	AudioFrameSpeech = 1,
	AudioFrameCN = 2, // Comfort Noise, https://tools.ietf.org/html/rfc3389
	VideoFrameKey = 3,
	VideoFrameDelta = 4,
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
	EncodedFrame()
		: EncodedFrame(nullptr, 0, 0)
	{
	}

	EncodedFrame(uint8_t *buffer, size_t length, size_t size)
		: buffer(buffer), length(length), size(size)
	{
	}

	uint32_t encoded_width = 0;
	uint32_t encoded_height = 0;
	uint32_t time_stamp = 0;
	FrameType frame_type = FrameType::VideoFrameDelta;
	uint8_t *buffer;
	size_t length;
	size_t size;
	bool complete_frame = false;
};

struct CodecSpecificInfoVP8
{
	int16_t picture_id;          // Negative value to skip pictureId.
	bool non_reference;
	uint8_t simulcast_idx;
	uint8_t temporal_idx;
	bool layer_sync;
	int tl0_pic_idx;         // Negative value to skip tl0PicIdx.
	int8_t key_idx;            // Negative value to skip keyIdx.
};

struct CodecSpecificInfoGeneric
{
	uint8_t simulcast_idx;      // TODO: 향후 확장 구현에서 사용
};

union CodecSpecificInfoUnion
{
	CodecSpecificInfoGeneric generic;
	CodecSpecificInfoVP8 vp8;
	// In the future
	// RTPVideoHeaderVP9 vp9;
};

struct CodecSpecificInfo
{
	CodecSpecificInfo() : codec_type(VideoCodecType::Unknown), codec_name(nullptr)
	{
	}

	VideoCodecType codec_type;
	const char *codec_name;
	CodecSpecificInfoUnion codec_specific;
};