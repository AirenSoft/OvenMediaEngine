#include "audio_specific_config.h"

#include <base/ovlibrary/bit_reader.h>
#include <base/ovlibrary/ovlibrary.h>

#include "base/ovlibrary/memory_view.h"

#define OV_LOG_TAG "AACSpecificConfig"

bool AudioSpecificConfig::IsValid() const
{
	if (_object_type == AacObjectType::AacObjectTypeNull ||
		_sampling_frequency_index == AacSamplingFrequencies::RATES_RESERVED ||
		_channel == 15)
	{
		return false;
	}

	return true;
}

ov::String AudioSpecificConfig::GetCodecsParameter() const
{
	// "mp4a.oo[.A]"
	//   oo = OTI = [Object Type Indication]
	//   A = [Object Type Number]
	//
	// OTI == 40 (Audio ISO/IEC 14496-3)
	//   http://mp4ra.org/#/object_types
	//
	// OTN == profile_number
	//   https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#MPEG-4_audio
	return ov::String::FormatString("mp4a.40.%d", static_cast<int>(_object_type));
}

bool AudioSpecificConfig::Parse(const std::shared_ptr<ov::Data> &data)
{
	if (data->GetLength() < MIN_AAC_SPECIFIC_CONFIG_SIZE)
	{
		logte("The data inputted is too small for parsing (%d must be bigger than %d)", data->GetLength(), MIN_AAC_SPECIFIC_CONFIG_SIZE);
		return false;
	}

	BitReader parser(data->GetDataAs<uint8_t>(), data->GetLength());

	_object_type = static_cast<AacObjectType>(parser.ReadBits<uint8_t>(5));
	_sampling_frequency_index = static_cast<AacSamplingFrequencies>(parser.ReadBits<uint8_t>(4));
	_channel = parser.ReadBits<uint8_t>(4);

	return true;
}

bool AudioSpecificConfig::Equals(const std::shared_ptr<DecoderConfigurationRecord> &other) 
{
	if (other == nullptr)
	{
		return false;
	}

	auto other_config = std::dynamic_pointer_cast<AudioSpecificConfig>(other);
	if (other_config == nullptr)
	{
		return false;
	}

	if(ObjectType() != other_config->ObjectType())
	{
		return false;
	}

	if(SamplingFrequency() != other_config->SamplingFrequency())
	{
		return false;
	}

	if(Channel() != other_config->Channel())
	{
		return false;
	}

	return true;
}

std::shared_ptr<ov::Data> AudioSpecificConfig::Serialize()
{
	ov::BitWriter bits(2);

	bits.WriteBits(5, _object_type);
	bits.WriteBits(4, _sampling_frequency_index);
	bits.WriteBits(4, _channel);

	return std::make_shared<ov::Data>(bits.GetData(), bits.GetDataSize());
}

AacObjectType AudioSpecificConfig::ObjectType() const
{
	return _object_type;
}

void AudioSpecificConfig::SetOjbectType(AacObjectType object_type)
{
	_object_type = object_type;
}

uint32_t AudioSpecificConfig::SamplerateNum() const
{
	switch (SamplingFrequency())
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
		case RATES_RESERVED:
		case EXPLICIT_RATE:
			return 0;
	}

	return 0;
}

AacSamplingFrequencies AudioSpecificConfig::SamplingFrequency() const
{
	return _sampling_frequency_index;
}

void AudioSpecificConfig::SetSamplingFrequency(AacSamplingFrequencies sampling_frequency_index)
{
	_sampling_frequency_index = sampling_frequency_index;
}

uint8_t AudioSpecificConfig::Channel() const
{
	return _channel;
}

void AudioSpecificConfig::SetChannel(uint8_t channel)
{
	_channel = channel;
}

AacProfile AudioSpecificConfig::GetAacProfile() const
{
	switch (_object_type)
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

ov::String AudioSpecificConfig::GetInfoString() const
{
	ov::String out_str = ov::String::FormatString("\n[AudioSpecificConfig]\n");

	out_str.AppendFormat("\tObjectType(%d)\n", ObjectType());
	out_str.AppendFormat("\tSamplingFrequency(%d)\n", SamplingFrequency());
	out_str.AppendFormat("\tChannel(%d)\n", Channel());

	return out_str;
}
