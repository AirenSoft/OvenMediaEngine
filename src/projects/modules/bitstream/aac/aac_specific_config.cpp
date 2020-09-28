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

void AACSpecificConfig::Serialize(std::vector<uint8_t>& serialze)
{
	ov::BitWriter bits(2);

	bits.Write(5, _object_type);
	bits.Write(4, _sampling_frequency_index);
	bits.Write(4, _channel);

	serialze.resize(bits.GetDataSize());
	std::copy(bits.GetData(), bits.GetData()+bits.GetDataSize(), serialze.begin());
}

AacObjectType AACSpecificConfig::ObjectType()
{
	return _object_type;
}


void AACSpecificConfig::SetOjbectType(AacObjectType object_type)
{
	_object_type = object_type;
}


AacSamplingFrequencies AACSpecificConfig::SamplingFrequency()
{
	return _sampling_frequency_index;
}


void AACSpecificConfig::SetSamplingFrequency(AacSamplingFrequencies sampling_frequency_index)
{
	_sampling_frequency_index= sampling_frequency_index;
}


uint8_t	AACSpecificConfig::Channel()
{
	return _channel;
}


void AACSpecificConfig::SetChannel(uint8_t channel)
{
	_channel = channel;
}


AacProfile AACSpecificConfig::GetAacProfile()
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


ov::String AACSpecificConfig::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[AACSpecificConfig]\n");

	out_str.AppendFormat("\tObjectType(%d)\n", ObjectType());
	out_str.AppendFormat("\tSamplingFrequency(%d)\n", SamplingFrequency());
	out_str.AppendFormat("\tChannel(%d)\n", Channel());

	return out_str;
}
