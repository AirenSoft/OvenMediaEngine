#include "h264_decoder_configuration_record.h"

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/bit_reader.h>

#define OV_LOG_TAG "AVCDecoderConfigurationRecord"

bool AVCDecoderConfigurationRecord::Parse(const uint8_t *data, size_t data_length, AVCDecoderConfigurationRecord &record)
{
	if(data_length < MIN_AVCDECODERCONFIGURATIONRECORD_SIZE)
	{
		logte("The data inputted is too small for parsing (%d must be bigger than %d)", data_length, MIN_AVCDECODERCONFIGURATIONRECORD_SIZE);
		return false;
	}

	BitReader parser(data, data_length);

	record._version = parser.ReadBytes<uint8_t>();
	record._profile_indication = parser.ReadBytes<uint8_t>();
	record._profile_compatibility = parser.ReadBytes<uint8_t>();
	record._level_indication = parser.ReadBytes<uint8_t>();
	record._reserved1 = parser.ReadBits<uint8_t>(6);
	if(record._reserved1 != 0b111111)
	{
		return false;
	}
	record._lengthMinusOne = parser.ReadBits<uint8_t>(2);
	record._reserved2 = parser.ReadBits<uint8_t>(3);
	if(record._reserved2 != 0b111)
	{
		return false;
	}

	record._num_of_sps = parser.ReadBits<uint8_t>(5);
	for(int i=0; i<record._num_of_sps; i++)
	{
		uint16_t sps_length = parser.ReadBytes<uint16_t>();
		if(sps_length == 0 || parser.BytesReamined() < sps_length)
		{
			return false;
		}
		auto sps = std::make_shared<ov::Data>(parser.CurrentPosition(), sps_length);
		parser.SkipBytes(sps_length);
		record._sps_list.push_back(sps);
	}

	record._num_of_pps = parser.ReadBytes<uint8_t>();
	for(int i=0; i<record._num_of_pps; i++)
	{
		uint16_t pps_length = parser.ReadBytes<uint16_t>();
		if(pps_length == 0 || parser.BytesReamined() < pps_length)
		{
			return false;
		}

		auto pps = std::make_shared<ov::Data>(parser.CurrentPosition(), pps_length);
		parser.SkipBytes(pps_length);
		record._pps_list.push_back(pps);
	}

	if(record._profile_indication == 100 || record._profile_indication == 110 || 
		record._profile_indication == 122 || record._profile_indication == 144)
	{
		record._reserved3 = parser.ReadBits<uint8_t>(6);
		record._chroma_format = parser.ReadBits<uint8_t>(2);
		record._reserved4 = parser.ReadBits<uint8_t>(5);
		record._bit_depth_luma_minus8 = parser.ReadBits<uint8_t>(3);

		record._num_of_sps_ext = parser.ReadBytes<uint8_t>();
		for(int i=0; i<record._num_of_sps_ext; i++)
		{
			uint16_t sps_ext_length = parser.ReadBytes<uint16_t>();
			if(sps_ext_length == 0 || parser.BytesReamined() < sps_ext_length)
			{
				return false;
			}

			auto sps_ext = std::make_shared<ov::Data>(parser.CurrentPosition(), sps_ext_length);
			parser.SkipBytes(sps_ext_length);
			record._sps_ext_list.push_back(sps_ext);
		}	
	}

	return true;
}

uint8_t AVCDecoderConfigurationRecord::Version()
{
	return _version;
}

uint8_t	AVCDecoderConfigurationRecord::ProfileIndication()
{
	return _profile_indication;
}

uint8_t AVCDecoderConfigurationRecord::Compatibility()
{
	return _profile_compatibility;
}

uint8_t AVCDecoderConfigurationRecord::LevelIndication()
{
	return _level_indication;
}

uint8_t AVCDecoderConfigurationRecord::LengthOfNALUnit()
{
	return _lengthMinusOne;	
}

uint8_t AVCDecoderConfigurationRecord::NumOfSPS()
{
	return _num_of_sps;
}

uint8_t AVCDecoderConfigurationRecord::NumOfPPS()
{
	return _num_of_pps;
}

uint8_t AVCDecoderConfigurationRecord::NumofSPSExt()
{
	return _num_of_sps_ext;
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::GetSPS(int index)
{
	if(static_cast<int>(_sps_list.size()) - 1 < index)
	{
		return nullptr;
	}

	return _sps_list[index];
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::GetPPS(int index)
{
	if(static_cast<int>(_pps_list.size()) - 1 < index)
	{
		return nullptr;
	}

	return _pps_list[index];
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::GetSPSExt(int index)
{
	if(static_cast<int>(_sps_ext_list.size()) - 1 < index)
	{
		return nullptr;
	}

	return _sps_ext_list[index];
}

uint8_t AVCDecoderConfigurationRecord::ChromaFormat()
{
	return _chroma_format;
}

uint8_t AVCDecoderConfigurationRecord::BitDepthLumaMinus8()
{
	return _bit_depth_luma_minus8;
}

std::vector<uint8_t> AVCDecoderConfigurationRecord::Serialize() const
{
	std::vector<uint8_t> stream;

	// TODO

	return stream;
}

bool AVCDecoderConfigurationRecord::Deserialize(const std::vector<uint8_t> &stream)
{
	
	// TODO

	return true;    
}

ov::String AVCDecoderConfigurationRecord::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[AVCDecoderConfigurationRecord]\n");

	out_str.AppendFormat("\tVersion(%d)\n", Version());
	out_str.AppendFormat("\tProfileIndication(%d)\n", ProfileIndication());
	out_str.AppendFormat("\tCompatibility(%d)\n", Compatibility());
	out_str.AppendFormat("\tLevelIndication(%d)\n", LevelIndication());
	out_str.AppendFormat("\tLengthOfNALUnit(%d)\n", LengthOfNALUnit());
	out_str.AppendFormat("\tNumOfSPS(%d)\n", NumOfSPS());
	out_str.AppendFormat("\tNumOfPPS(%d)\n", NumOfPPS());
	out_str.AppendFormat("\tNumofSPSExt(%d)\n", NumofSPSExt());
	out_str.AppendFormat("\tChromaFormat(%d)\n", ChromaFormat());
	out_str.AppendFormat("\tBitDepthLumaMinus8(%d)\n", BitDepthLumaMinus8());

	return out_str;
}
