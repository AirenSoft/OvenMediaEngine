//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include <cstdint>
#include <vector>

// Splits a buffer of back-to-back ADTS frames (an MPEG-TS PES, or the ADTS buffer assembled from an RTP packet) into its frames.
// An fMP4 sample must hold one AAC frame, and strict decoders (e.g. Safari) reject a sample of several glued frames.
class AacAdtsSplitter
{
public:
	struct Frame
	{
		size_t offset = 0;
		size_t length = 0;
		// Added to the buffer's timestamp: the frame index times the duration of one frame.
		int64_t timestamp_offset = 0;
	};

	struct Result
	{
		// Empty when the first frame cannot be split or timed; the caller then forwards the buffer as a whole.
		std::vector<Frame> frames;
		// Why the buffer was not split to its end, or empty.
		ov::String stop_reason;
	};

public:
	// `timescale` is the track's timebase denominator.
	// A non-positive `samples_per_frame` means 1024, the frame size of AAC-LC.
	static Result Split(const uint8_t *data, size_t length, int32_t timescale, int samples_per_frame);
};
