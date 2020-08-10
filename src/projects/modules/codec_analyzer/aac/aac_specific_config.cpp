#include "aac_specific_config.h"
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/bit_reader.h>

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