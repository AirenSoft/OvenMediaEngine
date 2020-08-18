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

	config._object_type = static_cast<WellKnownAACObjectTypes>(parser.ReadBits<uint8_t>(5));
	config._sampling_frequency_index = static_cast<AACSamplingFrequencies>(parser.ReadBits<uint8_t>(4));
	config._channel = parser.ReadBits<uint8_t>(4);

	return true;
}

WellKnownAACObjectTypes	AACSpecificConfig::ObjectType()
{
	return _object_type;
}

AACSamplingFrequencies AACSpecificConfig::SamplingFrequency()
{
	return _sampling_frequency_index;
}

uint8_t	AACSpecificConfig::Channel()
{
	return _channel;
}

std::vector<uint8_t> AACSpecificConfig::Serialize() const
{
	size_t size = 3;
	std::vector<uint8_t> stream(size);
    MemoryView memory_view(stream.data(), size);

	memory_view << (uint8_t)_object_type << (uint8_t)_sampling_frequency_index << (uint8_t)_channel;

	OV_ASSERT2(memory_view.good());

	return stream;
}

bool AACSpecificConfig::Deserialize(const std::vector<uint8_t> &stream)
{
    MemoryView memory_view(const_cast<uint8_t*>(stream.data()), stream.size(), stream.size());
    memory_view >> _object_type >> _sampling_frequency_index >> _channel;
    const bool result = memory_view.good() && memory_view.eof();
    OV_ASSERT2(result);
    return result;	
}


ov::String AACSpecificConfig::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[AACSpecificConfig]\n");

	out_str.AppendFormat("\tObjectType(%d)\n", ObjectType());
	out_str.AppendFormat("\tSamplingFrequency(%d)\n", SamplingFrequency());
	out_str.AppendFormat("\tChannel(%d)\n", Channel());

	return out_str;
}


