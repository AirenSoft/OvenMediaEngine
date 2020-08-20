#include "aac_adts.h"

#include <memory>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/log.h>
#include <base/ovlibrary/bit_reader.h>

#define OV_LOG_TAG "AACAdts"

bool AACAdts::IsValid(const uint8_t *data, size_t data_length)
{
    if(data == nullptr)
    {
        return false;
    }

	if(data_length < ADTS_MIN_SIZE)
	{
		return false;
	}
	
    // check syncword
    if( data[0] == 0xff && (data[1] & 0xf0) == 0xf0)
    {
        return true;
    }

    return false;
}

bool AACAdts::Parse(const uint8_t *data, size_t data_length, AACAdts &adts)
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

ov::String AACAdts::ProfileString()
{
	switch(Profile())
	{
		case AacObjectTypeAacMain: 
			return "AAC Main";
		case AacObjectTypeAacLC: 
			return "AAC LC";
		case AacObjectTypeAacSSR: 
			return "AAC SSR";
		case AacObjecttypeAacLTP: 
			return "AAC LTP";	
		case AacObjectTypeAacHE:
			return "AAC HE";
		case AacObjectTypeAacHEV2:
			return "AAC HEv2";			
	}

	return "Unkwnon";
}

AacSamplingFrequencies AACAdts::Samplerate()
{
	return static_cast<AacSamplingFrequencies>(_sampling_frequency_index);
}

uint32_t AACAdts::SamplerateNum()
{
	switch(Samplerate())
	{
		case RATES_96000HZ:
			return 96000;
		case RATES_88200HZ:
			return 88200;
		case RATES_64000HZ:
			return 64000;
		case RATES_48000HZ:
			return 48000;
		case RATES_44100HZ:
			return 44100;
		case RATES_32000HZ:
			return 32000;
		case RATES_24000HZ:
			return 24000;
		case RATES_22050HZ:
			return 22050;
		case RATES_16000HZ:
			return 16000;
		case RATES_12000HZ:
			return 12000;
		case RATES_11025HZ:
			return 11025;
		case RATES_8000HZ:
			return 8000;
		case RATES_7350HZ:
			return 7350;
		case EXPLICIT_RATE:
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

ov::String AACAdts::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[AACAdts]\n");

	out_str.AppendFormat("\tId(%d)\n", Id());
	out_str.AppendFormat("\tLayer(%d)\n", Layer());
	out_str.AppendFormat("\tProtectionAbsent(%s)\n", ProtectionAbsent()?"true":"false");
	out_str.AppendFormat("\tProfile(%d/%s)\n", Profile(), ProfileString().CStr());
	out_str.AppendFormat("\tSamplerate(%d/%d)\n", Samplerate(), SamplerateNum());
	out_str.AppendFormat("\tChannelConfiguration(%d)\n", ChannelConfiguration());
	out_str.AppendFormat("\tHome(%s)\n", Home()?"true":"false");
	out_str.AppendFormat("\tAacFrameLength(%d)\n", AacFrameLength());

	return out_str;
}
