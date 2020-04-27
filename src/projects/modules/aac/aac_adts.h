#pragma once

#include <base/ovlibrary/ovlibrary.h>


class AACAdts
{
	typedef struct adts_fixed_header {
		unsigned short syncword:12;
		unsigned char id:1;
		unsigned char layer:2;
		unsigned char protection_absent:1;
		unsigned char profile:2;
		unsigned char sampling_frequency_index: 4;
		unsigned char private_bit:1;
		unsigned char channel_configuration3;
		unsigned char original_copy:1;
		unsigned char home:1;
	} adts_fixed_header; // length : 28 bits

	typedef struct adts_variable_header {
		unsigned char copyright_identification_bit:1;
		unsigned char copyright_identification_start:1;
		unsigned short aac_frame_length:13;
		unsigned short adts_buffer_fullness:11;
		unsigned char number_of_raw_data_blocks_in_frame:2;
	} adts_variable_header; // length : 28 bits

	// MPEG-4 Audio Object Types:
	enum AacObjectType
	{
	    AacObjectTypeUnknown = -99,
	    // Table 1.1 - Audio Object Type definition
	    // @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	    AacObjectTypeAacMain = 0,
	    AacObjectTypeAacLC = 1,
	    AacObjectTypeAacSSR = 2,
	    // AAC HE = LC+SBR
	    AacObjectTypeAacHE = 4,
	    // AAC HEv2 = LC+SBR+PS
	    AacObjectTypeAacHEV2 = 28,
	};		

	// Sampling Frequencies
	enum SamplingFrequencies
	{
		Samplerate_96000 = 0,
		Samplerate_88200 = 1,
		Samplerate_64000 = 2,
		Samplerate_48000 = 3,
		Samplerate_44100 = 4,
		Samplerate_32000 = 5,
		Samplerate_24000 = 6,
		Samplerate_22050 = 7,
		Samplerate_16000 = 8,
		Samplerate_12000 = 9,
		Samplerate_11025 = 10,
		Samplerate_8000 = 11,
		Samplerate_7350 = 12,
		Samplerate_Unknown = 13
	};

public:
	static bool AppendAdtsHeader(int32_t ff_profile, int32_t ff_samplerate, int32_t ff_channels, std::shared_ptr<ov::Data> &media_packet_data);
};