#include "aac_adts.h"

#include <memory>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/log.h>
#include <base/ovlibrary/bit_reader.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

#define OV_LOG_TAG "AACAdts"


bool AACAdts::ParseAdtsHeader(const uint8_t *data, size_t data_length, AACAdts &adts)
{
	if(data_length < ADTS_MIN_SIZE)
	{
		return false;
	}

	BitReader parser(data, data_length);

	adts._syncword = parser.ReadBits<uint16_t>(12);
	if(adts._syncword != 0xFFF)
	{
		return false;
	}

	adts._id = parser.ReadBoolBit();
	adts._layer = parser.ReadBits<uint8_t>(2);
	adts._protection_absent = parser.ReadBoolBit();
	adts._profile = parser.ReadBits<uint8_t>(2);
	adts._sampling_frequency_index = parser.ReadBits<uint8_t>(4);
	adts._private_bit = parser.ReadBit();
	adts._channel_configuration = parser.ReadBits<uint8_t>(3);
	adts._original_copy = parser.ReadBoolBit();
	adts._home = parser.ReadBoolBit();

	adts._copyright_identification_bit = parser.ReadBoolBit();
	adts._copyright_identification_start = parser.ReadBoolBit();
	adts._aac_frame_length = parser.ReadBits<uint16_t>(13);
	adts._adts_buffer_fullness = parser.ReadBits<uint16_t>(11);
	adts._number_of_raw_data_blocks_in_frame = parser.ReadBits<uint8_t>(2);

	if(adts._protection_absent == false)
	{
		adts._crc = parser.ReadBits<uint16_t>(16);
	}

	return true;
}

uint8_t AACAdts::Id()
{	
	return _id;
}

uint8_t AACAdts::Layer()
{
	return _layer;
}

bool AACAdts::ProtectionAbsent()
{
	return _protection_absent;
}

AacObjectType AACAdts::Profile()
{
	return static_cast<AacObjectType>(_profile + 1);
}

SamplingFrequencies AACAdts::Samplerate()
{
	return static_cast<SamplingFrequencies>(_sampling_frequency_index);
}

uint32_t AACAdts::SamplerateNum()
{
	switch(Samplerate())
	{
		case Samplerate_96000:
			return 96000;
		case Samplerate_88200:
			return 88200;
		case Samplerate_64000:
			return 64000;
		case Samplerate_48000:
			return 48000;
		case Samplerate_44100:
			return 44100;
		case Samplerate_32000:
			return 32000;
		case Samplerate_24000:
			return 24000;
		case Samplerate_22050:
			return 22050;
		case Samplerate_16000:
			return 16000;
		case Samplerate_12000:
			return 12000;
		case Samplerate_11025:
			return 11025;
		case Samplerate_8000:
			return 8000;
		case Samplerate_7350:
			return 7350;
		case Samplerate_Unknown:
			return 0;
	}

	return 0;
}

uint8_t AACAdts::ChannelConfiguration()
{
	return _channel_configuration;
}

bool AACAdts::Originality()
{
	return _original_copy;
}

bool AACAdts::Home()
{
	return _home;
}

uint16_t AACAdts::AacFrameLength()
{
	return _aac_frame_length;
}

// TODO(Soulk): The parameters ff_profile and ff_samplerate parameters should be changed to AacObjectType and SamplingFrequencies
// Member variables in the member function must force the members of this class to use.
// Externally defined values should not be used (for modulation).
bool AACAdts::AppendAdtsHeader(AacObjectType profile, SamplingFrequencies samplerate, int32_t channels, std::shared_ptr<ov::Data> &media_packet_data)
{
	uint8_t aac_profile = (uint8_t)profile;
	int8_t aac_sample_rate = (int8_t)samplerate;
	int8_t aac_channels = (int8_t)channels;
   
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
