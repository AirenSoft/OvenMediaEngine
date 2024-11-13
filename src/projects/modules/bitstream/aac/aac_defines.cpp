#include "aac_defines.h"

#define _SAMPLE_RATE_CASE(rate, value) \
	case AacSamplingFrequencies::rate: \
		return value;

uint32_t GetAacSamplingFrequency(AacSamplingFrequencies sampling_frequency_index)
{
	switch (sampling_frequency_index)
	{
		_SAMPLE_RATE_CASE(_96000, 96000);
		_SAMPLE_RATE_CASE(_88200, 88200);
		_SAMPLE_RATE_CASE(_64000, 64000);
		_SAMPLE_RATE_CASE(_48000, 48000);
		_SAMPLE_RATE_CASE(_44100, 44100);
		_SAMPLE_RATE_CASE(_32000, 32000);
		_SAMPLE_RATE_CASE(_24000, 24000);
		_SAMPLE_RATE_CASE(_22050, 22050);
		_SAMPLE_RATE_CASE(_16000, 16000);
		_SAMPLE_RATE_CASE(_12000, 12000);
		_SAMPLE_RATE_CASE(_11025, 11025);
		_SAMPLE_RATE_CASE(_8000, 8000);
		_SAMPLE_RATE_CASE(_7350, 7350);
		case AacSamplingFrequencies::RESERVED1:
			[[fallthrough]];
		case AacSamplingFrequencies::RESERVED2:
			break;

		case AacSamplingFrequencies::ESCAPE_VALUE:
			// ESCAPE_VALUE must be handled in the if statement above
			OV_ASSERT2(false);
			break;
	}

	return 0;
}
