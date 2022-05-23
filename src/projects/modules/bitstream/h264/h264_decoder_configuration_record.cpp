#include "h264_decoder_configuration_record.h"

#include <base/ovlibrary/bit_reader.h>
#include <base/ovlibrary/bit_writer.h>
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "AVCDecoderConfigurationRecord"

bool AVCDecoderConfigurationRecord::Parse(const uint8_t *data, size_t data_length, AVCDecoderConfigurationRecord &record)
{
	if (data_length < MIN_AVCDECODERCONFIGURATIONRECORD_SIZE)
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
	if (record._reserved1 != 0b111111)
	{
		return false;
	}
	record._lengthMinusOne = parser.ReadBits<uint8_t>(2);
	record._reserved2 = parser.ReadBits<uint8_t>(3);
	if (record._reserved2 != 0b111)
	{
		return false;
	}

	record._num_of_sps = parser.ReadBits<uint8_t>(5);
	for (int i = 0; i < record._num_of_sps; i++)
	{
		uint16_t sps_length = parser.ReadBytes<uint16_t>();
		if (sps_length == 0 || parser.BytesReamined() < sps_length)
		{
			return false;
		}
		auto sps = std::make_shared<ov::Data>(parser.CurrentPosition(), sps_length);
		parser.SkipBytes(sps_length);
		record._sps_list.push_back(sps);
	}

	record._num_of_pps = parser.ReadBytes<uint8_t>();
	for (int i = 0; i < record._num_of_pps; i++)
	{
		uint16_t pps_length = parser.ReadBytes<uint16_t>();
		if (pps_length == 0 || parser.BytesReamined() < pps_length)
		{
			return false;
		}

		auto pps = std::make_shared<ov::Data>(parser.CurrentPosition(), pps_length);
		parser.SkipBytes(pps_length);
		record._pps_list.push_back(pps);
	}

	if (record._profile_indication == 100 || record._profile_indication == 110 ||
		record._profile_indication == 122 || record._profile_indication == 144)
	{
		record._reserved3 = parser.ReadBits<uint8_t>(6);
		record._chroma_format = parser.ReadBits<uint8_t>(2);
		record._reserved4 = parser.ReadBits<uint8_t>(5);
		record._bit_depth_luma_minus8 = parser.ReadBits<uint8_t>(3);
		record._reserved5 = parser.ReadBits<uint8_t>(5);
		record._bit_depth_chroma_minus8 = parser.ReadBits<uint8_t>(3);

		record._num_of_sps_ext = parser.ReadBytes<uint8_t>();
		for (int i = 0; i < record._num_of_sps_ext; i++)
		{
			uint16_t sps_ext_length = parser.ReadBytes<uint16_t>();
			if (sps_ext_length == 0 || parser.BytesReamined() < sps_ext_length)
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

bool AVCDecoderConfigurationRecord::Parse(const std::vector<uint8_t> &data, AVCDecoderConfigurationRecord &record)
{
	if (data.size() == 0)
	{
		return false;
	}

	return Parse(data.data(), data.size(), record);
}

uint8_t AVCDecoderConfigurationRecord::Version()
{
	return _version;
}

uint8_t AVCDecoderConfigurationRecord::ProfileIndication()
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
	if (static_cast<int>(_sps_list.size()) - 1 < index)
	{
		return nullptr;
	}

	return _sps_list[index];
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::GetPPS(int index)
{
	if (static_cast<int>(_pps_list.size()) - 1 < index)
	{
		return nullptr;
	}

	return _pps_list[index];
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::GetSPSExt(int index)
{
	if (static_cast<int>(_sps_ext_list.size()) - 1 < index)
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

uint8_t AVCDecoderConfigurationRecord::BitDepthChromaMinus8()
{
	return _bit_depth_chroma_minus8;
}
/* ISO 14496-15, 5.2.4.1
aligned(8) class AVCDecoderConfigurationRecord {
	unsigned int(8) configurationVersion = 1;
	unsigned int(8) AVCProfileIndication;
	unsigned int(8) profile_compatibility;
	unsigned int(8) AVCLevelIndication;
	bit(6) reserved = ‘111111’b;
	unsigned int(2) lengthSizeMinusOne;
	bit(3) reserved = ‘111’b;
	unsigned int(5) numOfSequenceParameterSets;
	for (i=0; i< numOfSequenceParameterSets;  i++) {
		unsigned int(16) sequenceParameterSetLength ;
		bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
	}
	unsigned int(8) numOfPictureParameterSets;
	for (i=0; i< numOfPictureParameterSets;  i++) {
		unsigned int(16) pictureParameterSetLength;
		bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
	}
	if( profile_idc  ==  100  ||  profile_idc  ==  110  ||
	    profile_idc  ==  122  ||  profile_idc  ==  144 )
	{
		bit(6) reserved = ‘111111’b;
		unsigned int(2) chroma_format;
		bit(5) reserved = ‘11111’b;
		unsigned int(3) bit_depth_luma_minus8;
		bit(5) reserved = ‘11111’b;
		unsigned int(3) bit_depth_chroma_minus8;
		unsigned int(8) numOfSequenceParameterSetExt;
		for (i=0; i< numOfSequenceParameterSetExt; i++) {
			unsigned int(16) sequenceParameterSetExtLength;
			bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
		}
	}
} */

std::tuple<std::shared_ptr<ov::Data>, FragmentationHeader> AVCDecoderConfigurationRecord::GetSpsPpsAsAnnexB(uint8_t start_code_size)
{
	auto data = std::make_shared<ov::Data>(1024);
	FragmentationHeader frag_header;
	size_t offset = 0;

	uint8_t START_CODE[4];

	START_CODE[0] = 0x00;
	START_CODE[1] = 0x00;
	if (start_code_size == 3)
	{
		START_CODE[2] = 0x01;
	}
	else  // 4
	{
		START_CODE[2] = 0x00;
		START_CODE[3] = 0x01;
	}

	for (int i = 0; i < NumOfSPS(); i++)
	{
		data->Append(START_CODE, start_code_size);
		offset += start_code_size;

		auto sps = GetSPS(i);
		frag_header.fragmentation_offset.push_back(offset);
		frag_header.fragmentation_length.push_back(sps->GetLength());

		offset += sps->GetLength();

		data->Append(sps);
	}

	for (int i = 0; i < NumOfPPS(); i++)
	{
		data->Append(START_CODE, start_code_size);
		offset += start_code_size;

		auto pps = GetPPS(i);
		frag_header.fragmentation_offset.push_back(offset);
		frag_header.fragmentation_length.push_back(pps->GetLength());
		data->Append(pps);
	}

	return {data, frag_header};
}

std::shared_ptr<ov::Data> AVCDecoderConfigurationRecord::Serialize()
{
	ov::BitWriter bits(512);

	bits.Write(8, Version());
	bits.Write(8, ProfileIndication());
	bits.Write(8, Compatibility());
	bits.Write(8, LevelIndication());
	bits.Write(6, 0x3F);  // 111111'b
	bits.Write(2, LengthOfNALUnit());
	bits.Write(3, 0x07);		// 111b
	bits.Write(5, NumOfSPS());	// num of SPS
	for (auto i = 0; i < NumOfSPS(); i++)
	{
		auto sps = GetSPS(i);
		bits.Write(16, sps->GetLength());  // sps length
		for (size_t j = 0; j < sps->GetLength(); j++)
		{
			bits.Write(8, (uint32_t)sps->AtAs<uint8_t>(j));
		}
	}
	bits.Write(8, NumOfPPS());	// num of PPS
	for (auto i = 0; i < NumOfPPS(); i++)
	{
		auto pps = GetPPS(i);
		bits.Write(16, pps->GetLength());  // pps length
		for (size_t j = 0; j < pps->GetLength(); j++)
		{
			bits.Write(8, (uint32_t)pps->AtAs<uint8_t>(j));
		}
	}

	return std::make_shared<ov::Data>(bits.GetData(), bits.GetDataSize());
}

void AVCDecoderConfigurationRecord::Serialize(std::vector<uint8_t> &serialze)
{
	auto data = Serialize();
	serialze.resize(data->GetLength());
	std::copy(data->GetDataAs<uint8_t>(), data->GetDataAs<uint8_t>() + data->GetLength(), serialze.begin());
}

void AVCDecoderConfigurationRecord::SetVersion(uint8_t version)
{
	_version = version;
}

void AVCDecoderConfigurationRecord::SetProfileIndication(uint8_t profile_indiciation)
{
	_profile_indication = profile_indiciation;
}

void AVCDecoderConfigurationRecord::SetCompatibility(uint8_t profile_compatibility)
{
	_profile_compatibility = profile_compatibility;
}

void AVCDecoderConfigurationRecord::SetlevelIndication(uint8_t level_indication)
{
	_level_indication = level_indication;
}

void AVCDecoderConfigurationRecord::SetLengthOfNalUnit(uint8_t lengthMinusOne)
{
	_lengthMinusOne = lengthMinusOne;
}

void AVCDecoderConfigurationRecord::AddSPS(std::shared_ptr<ov::Data> sps)
{
	_sps_list.push_back(sps);
	_num_of_sps++;
}

void AVCDecoderConfigurationRecord::AddPPS(std::shared_ptr<ov::Data> pps)
{
	_pps_list.push_back(pps);
	_num_of_pps++;
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
	out_str.AppendFormat("\tBitDepthChromaMinus8(%d)\n", BitDepthChromaMinus8());

	return out_str;
}
