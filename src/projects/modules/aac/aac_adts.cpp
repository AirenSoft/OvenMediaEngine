#include "aac_adts.h"

#include <memory>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/log.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

#define OV_LOG_TAG "AACAdts"

bool AACAdts::AppendAdtsHeader(int32_t ff_profile, int32_t ff_samplerate, int32_t ff_channels, std::shared_ptr<ov::Data> &media_packet_data)
{
	int8_t aac_profile = (int8_t)(
		(ff_profile == FF_PROFILE_AAC_MAIN)?AacObjectTypeAacMain:
		(ff_profile == FF_PROFILE_AAC_LOW)?AacObjectTypeAacLC:
		(ff_profile == FF_PROFILE_AAC_SSR)?AacObjectTypeAacSSR:
		(ff_profile == FF_PROFILE_AAC_HE)?AacObjectTypeAacHE:
		(ff_profile == FF_PROFILE_AAC_HE_V2)?AacObjectTypeAacHEV2:AacObjectTypeUnknown
	);

	int8_t aac_sample_rate = 
		(ff_samplerate == 96000)?Samplerate_96000:
		(ff_samplerate == 88200)?Samplerate_88200:
		(ff_samplerate == 64000)?Samplerate_64000:
		(ff_samplerate == 48000)?Samplerate_48000:
		(ff_samplerate == 44100)?Samplerate_44100:
		(ff_samplerate == 32000)?Samplerate_32000:
		(ff_samplerate == 24000)?Samplerate_24000:
		(ff_samplerate == 22050)?Samplerate_22050:
		(ff_samplerate == 16000)?Samplerate_16000:
		(ff_samplerate == 12000)?Samplerate_12000:
		(ff_samplerate == 11025)?Samplerate_11025:
		(ff_samplerate == 7350)?Samplerate_7350:Samplerate_Unknown;

	int8_t aac_channels = ff_channels;

   
	uint8_t aac_header[7];

	uint8_t *pos = aac_header;
	int16_t aac_frame_length = media_packet_data->GetLength() + 7;

	// Syncword 12 bslbf
	*pos++ = 0xff;
	
	// 4bits left.
	// adts_fixed_header(), 1.A.2.2.1 Fixed Header of ADTS
	// ID 1 bslbf
	// Layer 2 uimsbf
	// protection_absent 1 bslbf
	*pos++ = 0xf1;

	// profile 2 uimsbf
	// sampling_frequency_index 4 uimsbf
	// private_bit 1 bslbf
	// channel_configuration 3 uimsbf
	// original/copy 1 bslbf
	// home 1 bslbf
	*pos++ = ((aac_profile << 6) & 0xc0) | ((aac_sample_rate << 2) & 0x3c) | ((aac_channels >> 2) & 0x01);

	// 4bits left.
	// adts_variable_header(), 1.A.2.2.2 Variable Header of ADTS
	// copyright_identification_bit 1 bslbf
	// copyright_identification_start 1 bslbf
	*pos++ = ((aac_channels << 6) & 0xc0) | ((aac_frame_length >> 11) & 0x03);

	// aac_frame_length 13 bslbf: Length of the frame including headers and error_check in bytes.
	// use the left 2bits as the 13 and 12 bit,
	// the aac_frame_length is 13bits, so we move 13-2=11.
	*pos++ = aac_frame_length >> 3;
	
	// adts_buffer_fullness 11 bslbf
	*pos++ = (aac_frame_length << 5) & 0xe0;

	// no_raw_data_blocks_in_frame 2 uimsbf
	*pos++ = 0xfc;

	media_packet_data->Insert(aac_header, 0, sizeof(aac_header));

	return true;
}