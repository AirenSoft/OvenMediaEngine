//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "aac_adts_splitter.h"

#include <base/mediarouter/media_type.h>

#include "aac_adts.h"

AacAdtsSplitter::Result AacAdtsSplitter::Split(const uint8_t *data, size_t length, int32_t timescale, int samples_per_frame)
{
	constexpr int AAC_LC_SAMPLES_PER_FRAME = 1024;

	Result result;

	if (samples_per_frame <= 0)
	{
		samples_per_frame = AAC_LC_SAMPLES_PER_FRAME;
	}

	// Resolved once from the first ADTS header; the sample rate is constant within a track.
	int64_t frame_duration = 0;
	size_t offset = 0;

	while ((offset + ADTS_MIN_SIZE) <= length)
	{
		AACAdts adts;

		if (AACAdts::Parse(data + offset, length - offset, adts) == false)
		{
			result.stop_reason.Format("stopped at offset %zu/%zu with no valid ADTS header; the input is likely truncated or corrupted", offset, length);
			break;
		}

		const size_t frame_length = adts.AacFrameLength();
		if ((frame_length < ADTS_MIN_SIZE) || ((offset + frame_length) > length))
		{
			result.stop_reason.Format("stopped at offset %zu: frame length %zu exceeds the buffer of %zu bytes; the input is likely truncated or corrupted", offset, frame_length, length);
			break;
		}

		// A reserved `sampling_frequency_index` gives a sample rate of 0, and the escape value trips `OV_ASSERT2()` in debug builds.
		// Frames that cannot be timed are not split, so they never share a timestamp.
		if (frame_duration == 0)
		{
			const auto samplerate = adts.Samplerate();

			if (samplerate != 0)
			{
				frame_duration = cmn::Rational::Rescale(samples_per_frame, cmn::Rational(1, static_cast<int32_t>(samplerate)), cmn::Rational(1, timescale));
			}

			if (frame_duration <= 0)
			{
				result.stop_reason.Format("not split: cannot resolve the frame duration (samplerate %u, timescale %d)", samplerate, timescale);
				break;
			}
		}

		result.frames.push_back({offset, frame_length, static_cast<int64_t>(result.frames.size()) * frame_duration});
		offset += frame_length;
	}

	// A mid-buffer stop above leaves `>= ADTS_MIN_SIZE` bytes, so only a tail shorter than a header is reported here.
	if ((result.frames.empty() == false) && (offset < length) && ((length - offset) < ADTS_MIN_SIZE))
	{
		result.stop_reason.Format("dropped %zu trailing byte(s) shorter than an ADTS header at offset %zu/%zu", length - offset, offset, length);
	}

	return result;
}
