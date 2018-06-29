#pragma once

#include <cstddef>
#include <cstdint>

enum VideoCodecType
{
	kVideoCodecVP8,
	kVideoCodecVP9,
	kVideoCodecH264,
	kVideoCodecI420,
	kVideoCodecRED,
	kVideoCodecULPFEC,
	kVideoCodecFlexfec,
	kVideoCodecGeneric,
	kVideoCodecStereo,
	kVideoCodecUnknown
};

enum FrameType
{
	kEmptyFrame = 0,
	kAudioFrameSpeech = 1,
	kAudioFrameCN = 2,
	kVideoFrameKey = 3,
	kVideoFrameDelta = 4,
};

class FragmentationHeader
{
public:
	FragmentationHeader()
	{
		fragmentationVectorSize = 0;
		fragmentationOffset = nullptr;
		fragmentationLength = nullptr;
		fragmentationTimeDiff = nullptr;
		fragmentationPlType = nullptr;
	}
	
	~FragmentationHeader()
	{
		delete[] fragmentationOffset;
		delete[] fragmentationLength;
		delete[] fragmentationTimeDiff;
		delete[] fragmentationPlType;
	}
	
	uint16_t	fragmentationVectorSize;	// Number of fragmentations
	size_t*		fragmentationOffset;		// Offset of pointer to data for each
	// fragmentation
	size_t*		fragmentationLength;		// Data size for each fragmentation
	uint16_t*	fragmentationTimeDiff;		// Timestamp difference relative "now" for
	// each fragmentation
	uint8_t*	fragmentationPlType;		// Payload type of each fragmentation
};


class EncodedFrame
{
public:
	EncodedFrame()
			: EncodedFrame(nullptr, 0, 0) {}
	EncodedFrame(uint8_t* buffer, size_t length, size_t size)
			: _buffer(buffer), _length(length), _size(size) {}
	
	uint32_t 	_encodedWidth = 0;
	uint32_t 	_encodedHeight = 0;
	uint32_t 	_timeStamp = 0;
	FrameType 	_frameType = kVideoFrameDelta;
	uint8_t* 	_buffer;
	size_t 		_length;
	size_t 		_size;
	bool 		_completeFrame = false;
};

struct CodecSpecificInfoVP8
{
    int16_t 	pictureId;          // Negative value to skip pictureId.
    bool    	nonReference;
    uint8_t 	simulcastIdx;
    uint8_t 	temporalIdx;
    bool    	layerSync;
    int     	tl0PicIdx;         // Negative value to skip tl0PicIdx.
    int8_t  	keyIdx;            // Negative value to skip keyIdx.
};

struct CodecSpecificInfoGeneric
{
    uint8_t 	simulcast_idx;      // TODO: 향후 확장 구현에서 사용
};

union CodecSpecificInfoUnion
{
    CodecSpecificInfoGeneric 	generic;
    CodecSpecificInfoVP8 		VP8;
	// In the future
	// RTPVideoHeaderVP9 VP9;
};

struct CodecSpecificInfo
{
    CodecSpecificInfo() : codecType(kVideoCodecUnknown), codec_name(nullptr) {}
    VideoCodecType codecType;
    const char* codec_name;
    CodecSpecificInfoUnion codecSpecific;
};