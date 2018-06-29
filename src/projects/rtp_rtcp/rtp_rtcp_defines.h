#pragma once

#include <stdint.h>
#include <cstdint>
#include <vector>
#include "base/ovlibrary/ovlibrary.h"
#include "rtp_packet.h"
#include "rtp_rtcp_interface.h"
#include "base/common_video.h"

enum RtpVideoCodecTypes
{
	kRtpVideoNone = 0,
	kRtpVideoVp8 = 1,
	// In the future
	// kRtpVideoVp9 = 2
	// kRtpVideoH264 = 3
};

const int16_t	kNoPictureId = -1;
const int16_t	kNoTl0PicIdx = -1;
const uint8_t	kNoTemporalIdx = 0xFF;
const int		kNoKeyIdx = -1;

struct RTPVideoHeaderVP8
{
	void InitRTPVideoHeaderVP8()
	{
		nonReference = false;
		pictureId = kNoPictureId;
		tl0PicIdx = kNoTl0PicIdx;
		temporalIdx = kNoTemporalIdx;
		layerSync = false;
		keyIdx = kNoKeyIdx;
		partitionId = 0;
		beginningOfPartition = false;
	}
	
	bool	nonReference;			// Frame is discardable.
	int16_t	pictureId;          	// Picture ID index, 15 bits;
	// kNoPictureId if PictureID does not exist.
	int16_t	tl0PicIdx;				// TL0PIC_IDX, 8 bits;
	// kNoTl0PicIdx means no value provided.
	uint8_t	temporalIdx;			// Temporal layer index, or kNoTemporalIdx.
	bool	layerSync;				// This frame is a layer sync frame.
	// Disabled if temporalIdx == kNoTemporalIdx.
	int		keyIdx;					// 5 bits; kNoKeyIdx means not used.
	int		partitionId;			// VP8 partition ID
	bool	beginningOfPartition;	// True if this packet is the first
	// in a VP8 partition. Otherwise false
};

union RTPVideoTypeHeader
{
	RTPVideoHeaderVP8 VP8;
	// In the future
	// RTPVideoHeaderVP9 VP9;
};

struct RTPVideoHeader
{
	uint16_t			width;  // size
	uint16_t			height;

	uint8_t 			simulcastIdx; // Extension, 0이면 사용하지 않음
	bool				is_first_packet_in_frame;

	RtpVideoCodecTypes 	codec;
	RTPVideoTypeHeader 	codecHeader;
};
