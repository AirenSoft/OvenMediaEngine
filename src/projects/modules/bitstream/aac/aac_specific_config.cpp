#include "aac_specific_config.h"
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/bit_reader.h>
#include "base/ovlibrary/memory_view.h"

#define OV_LOG_TAG "AACSpecificConfig"

bool AACSpecificConfig::Parse(const uint8_t *data, size_t data_length, AACSpecificConfig &config)
{
	if(data_length < MIN_AAC_SPECIFIC_CONFIG_SIZE)
	{
		logte("The data inputted is too small for parsing (%d must be bigger than %d)", data_length, MIN_AAC_SPECIFIC_CONFIG_SIZE);
		return false;
	}

	BitReader parser(data, data_length);

	config._object_type = static_cast<AacObjectType>(parser.ReadBits<uint8_t>(5));
	config._sampling_frequency_index = static_cast<AacSamplingFrequencies>(parser.ReadBits<uint8_t>(4));
	config._channel = parser.ReadBits<uint8_t>(4);

	return true;
}


// std::vector<uint8_t> AACSpecificConfig::Serialize() const
// {
// 	ov::BitWriter bits(2);

// 	bits.Write(5, _object_type);
// 	bits.Write(4, _sampling_frequency_index);
// 	bits.Write(4, _channel);

// 	std::vector<uint8_t> dest(bits.GetDataSize());
// 	std::copy(bits.GetData(), bits.GetData()+bits.GetDataSize(), dest.begin());

// 	return dest;
// }

std::shared_ptr<ov::Data> AACSpecificConfig::Serialize()
{
	ov::BitWriter bits(2);

	bits.Write(5, _object_type);
	bits.Write(4, _sampling_frequency_index);
	bits.Write(4, _channel);

	return std::make_shared<ov::Data>(bits.GetData(), bits.GetDataSize());
}

void AACSpecificConfig::Serialize(std::vector<uint8_t>& serialze)
{
	auto data = Serialize();
	serialze.resize(data->GetLength());
	std::copy(data->GetDataAs<uint8_t>(), data->GetDataAs<uint8_t>()+data->GetLength(), serialze.begin());
}

AacObjectType AACSpecificConfig::ObjectType() const
{
	return _object_type;
}


void AACSpecificConfig::SetOjbectType(AacObjectType object_type)
{
	_object_type = object_type;
}

uint32_t AACSpecificConfig::SamplerateNum() const
{
	switch(SamplingFrequency())
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

AacSamplingFrequencies AACSpecificConfig::SamplingFrequency() const
{
	return _sampling_frequency_index;
}


void AACSpecificConfig::SetSamplingFrequency(AacSamplingFrequencies sampling_frequency_index)
{
	_sampling_frequency_index= sampling_frequency_index;
}


uint8_t	AACSpecificConfig::Channel() const
{
	return _channel;
}


void AACSpecificConfig::SetChannel(uint8_t channel)
{
	_channel = channel;
}


AacProfile AACSpecificConfig::GetAacProfile() const
{
	switch(_object_type)
	{
		case AacObjectTypeAacMain:
			return AacProfileMain;
		case AacObjectTypeAacHE:
		case AacObjectTypeAacHEV2:
		case AacObjectTypeAacLC:
			return AacProfileLC;
		case AacObjectTypeAacSSR:
			return AacProfileSSR;
		default:
			return AacProfileReserved;
	}	
}


ov::String AACSpecificConfig::GetInfoString() const
{
	ov::String out_str = ov::String::FormatString("\n[AACSpecificConfig]\n");

	out_str.AppendFormat("\tObjectType(%d)\n", ObjectType());
	out_str.AppendFormat("\tSamplingFrequency(%d)\n", SamplingFrequency());
	out_str.AppendFormat("\tChannel(%d)\n", Channel());

	return out_str;
}
