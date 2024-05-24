#include <base/ovlibrary/ovlibrary.h>
#include <base/common_types.h>
#include "h265_decoder_configuration_record.h"

#define OV_LOG_TAG "HEVCDecoderConfigurationRecord"

bool HEVCDecoderConfigurationRecord::IsValid() const
{
	// VPS, SPS, PPS are mandatory
	if (_nal_units.find(static_cast<uint8_t>(H265NALUnitType::VPS)) == _nal_units.end() ||
		_nal_units.find(static_cast<uint8_t>(H265NALUnitType::SPS)) == _nal_units.end() ||
		_nal_units.find(static_cast<uint8_t>(H265NALUnitType::PPS)) == _nal_units.end())
	{
		return false;
	}
	
	return true;
}

int32_t HEVCDecoderConfigurationRecord::GetWidth()
{
	if (IsValid() == false)
	{
		return 0;
	}

	return _sps.GetWidth();
}

int32_t HEVCDecoderConfigurationRecord::GetHeight()
{
	if (IsValid() == false)
	{
		return 0;
	}

	return _sps.GetHeight();
}

ov::String HEVCDecoderConfigurationRecord::GetCodecsParameter() const
{
	// ISO 14496-15, Annex E.3
	// When the first element of a value is a code indicating a codec from the High Efficiency Video Coding specification (ISO/IEC 23008-2), as documented in clause 8 (such as 'hev1', 'hev2', 'hvc1', 'hvc2', 'shv1' or 'shc1'), the elements following are a series of values from the HEVC or SHVC decoder configuration record, separated by period characters (“.”). In all numeric encodings, leading zeroes may be omitted,

	// •	the general_profile_space, encoded as no character (general_profile_space == 0), or ‘A’, ‘B’, ‘C’ for general_profile_space 1, 2, 3, followed by the general_profile_idc encoded as a decimal number;
	
	// •	the general_profile_compatibility_flags, encoded in hexadecimal (leading zeroes may be omitted);
	
	// •	the general_tier_flag, encoded as ‘L’ (general_tier_flag==0) or ‘H’ (general_tier_flag==1), followed by the general_level_idc, encoded as a decimal number;
	
	// •	each of the 6 bytes of the constraint flags, starting from the byte containing the general_progressive_source_flag, each encoded as a hexadecimal number, and the encoding of each byte separated by a period; trailing bytes that are zero may be omitted.
	
	// Examples:
	// codecs=hev1.1.2.L93.B0
	// a progressive, non-packed stream, Main Profile, Main Tier, Level 3.1. (Only one byte of flags is given here).
	// codecs=hev1.A4.41.H120.B0.23
	// a (mythical) progressive, non-packed stream in profile space 1, with general_profile_idc 4, some compatibility flags set, and in High tier at Level 4 and two bytes of constraint flags supplied.

	// hev1.1.40000000.L120

	ov::String codecs_parameter;

	codecs_parameter += "hev1";

	if (_general_profile_space == 0)
	{
		codecs_parameter += ov::String::FormatString(".%d", _general_profile_idc);
	}
	else
	{
		codecs_parameter += ov::String::FormatString(".%c%d", 'A' + _general_profile_space - 1, _general_profile_idc);
	}

	// general_profile_compatibility_flags in in reverse order
	uint32_t reverse_value = 0;
	for (uint32_t i=0; i < 32; i++)
	{
		reverse_value <<= 1;
		reverse_value |= (_general_profile_compatibility_flags >> i) & 0x01;
	}

	codecs_parameter += ov::String::FormatString(".%x", reverse_value);
	codecs_parameter += ov::String::FormatString(".%c%d", _general_tier_flag == 0 ? 'L' : 'H', _general_level_idc);

	for (size_t i = 0; i < 6; ++i)
	{
		uint8_t flag = 0;
		// _general_constraint_indicator_flags has 48 bits (6 bytes) values
		// Get first 8 bits (1 byte) from _general_constraint_indicator_flags
		flag = (_general_constraint_indicator_flags >> (8 * (5 - i))) & 0xFF;

		if (flag == 0)
		{
			// Skip trailing zero bytes
			break;
		}

		codecs_parameter += ov::String::FormatString(".%x", flag);
	}

	return codecs_parameter;
}

bool HEVCDecoderConfigurationRecord::Parse(const std::shared_ptr<ov::Data> &data)
{
	if (data == nullptr)
	{
		return false;
	}

	// check length
	if (data->GetLength() < MIN_HEVCDECODERCONFIGURATIONRECORD_SIZE)
	{
		return false;
	}

	auto pdata = data->GetDataAs<uint8_t>();
	auto data_length = data->GetLength();

	BitReader parser(pdata, data_length);

	// configurationVersion
	_version = parser.ReadBytes<uint8_t>();

	// general_profile_space
	_general_profile_space = parser.ReadBits<uint8_t>(2);

	// general_tier_flag
	_general_tier_flag = parser.ReadBits<uint8_t>(1);

	// general_profile_idc
	_general_profile_idc = parser.ReadBits<uint8_t>(5);

	// general_profile_compatibility_flags
	_general_profile_compatibility_flags = parser.ReadBytes<uint32_t>();

	// general_constraint_indicator_flags
	uint32_t general_constraint_indicator_flags_high = parser.ReadBytes<uint32_t>();
	uint16_t general_constraint_indicator_flags_low = parser.ReadBytes<uint16_t>();

	_general_constraint_indicator_flags = (static_cast<uint64_t>(general_constraint_indicator_flags_high) << 16) | general_constraint_indicator_flags_low;

	// general_level_idc
	_general_level_idc = parser.ReadBytes<uint8_t>();

	// reserved
	parser.ReadBits<uint8_t>(4);

	// min_spatial_segmentation_idc
	_min_spatial_segmentation_idc = parser.ReadBits<uint16_t>(12);

	// reserved
	parser.ReadBits<uint8_t>(6);

	// parallelismType
	_parallelism_type = parser.ReadBits<uint8_t>(2);

	// reserved
	parser.ReadBits<uint8_t>(6);

	// chromaFormat
	_chroma_format = parser.ReadBits<uint8_t>(2);

	// reserved
	parser.ReadBits<uint8_t>(5);

	// bitDepthLumaMinus8
	_bit_depth_luma_minus8 = parser.ReadBits<uint8_t>(3);

	// reserved
	parser.ReadBits<uint8_t>(5);

	// bitDepthChromaMinus8
	_bit_depth_chroma_minus8 = parser.ReadBits<uint8_t>(3);

	// avgFrameRate
	_avg_frame_rate = parser.ReadBytes<uint16_t>();

	// constantFrameRate
	_constant_frame_rate = parser.ReadBits<uint8_t>(2);

	// numTemporalLayers
	_num_temporal_layers = parser.ReadBits<uint8_t>(3);

	// temporalIdNested
	_temporal_id_nested = parser.ReadBits<uint8_t>(1);

	// lengthSizeMinusOne
	_length_size_minus_one = parser.ReadBits<uint8_t>(2);

	// numOfArrays
	auto num_of_arrays = parser.ReadBits<uint8_t>(8);

	// check length
	// if (parser.BytesRemained() < num_of_arrays * 3)
	// {
	// 	return false;
	// }

	for (size_t i = 0; i < num_of_arrays; ++i)
	{
		// array_completeness
		[[maybe_unused]] auto array_completeness = parser.ReadBits<uint8_t>(1);

		// reserved
		parser.ReadBits<uint8_t>(1);

		// NAL_unit_type
		auto nal_unit_type = parser.ReadBits<uint8_t>(6);

		// numNalus
		auto num_nalus = parser.ReadBytes<uint16_t>();

		// check length
		if (parser.BytesRemained() < num_nalus)
		{
			return false;
		}

		for (size_t j = 0; j < num_nalus; ++j)
		{
			// nalUnitLength
			auto nal_unit_length = parser.ReadBytes<uint16_t>();

			// check length
			if (parser.BytesRemained() < nal_unit_length)
			{
				return false;
			}

			// nalUnit
			auto nal_unit = std::make_shared<ov::Data>(parser.CurrentPosition(), nal_unit_length);

			// add nalUnit to _nal_units
			auto &v = _nal_units[nal_unit_type];
			v.push_back(nal_unit);

			// skip nalUnit
			parser.SkipBytes(nal_unit_length);
		}
	}

	return true;
}

bool HEVCDecoderConfigurationRecord::Equals(const std::shared_ptr<DecoderConfigurationRecord> &other) 
{
	if (other == nullptr)
	{
		return false;
	}
	
	auto other_config = std::dynamic_pointer_cast<HEVCDecoderConfigurationRecord>(other);
	if (other_config == nullptr)
	{
		return false;
	}

	if (GeneralProfileIDC() != other_config->GeneralProfileIDC())
	{
		return false;
	}

	if (GeneralLevelIDC() != other_config->GeneralLevelIDC())
	{
		return false;
	}

	if(GetWidth() != other_config->GetWidth())
	{
		return false;
	}

	if(GetHeight() != other_config->GetHeight())
	{
		return false;
	}

	return true;
}

std::shared_ptr<ov::Data> HEVCDecoderConfigurationRecord::Serialize()
{
	if (IsValid() == false)
	{
		// VPS, SPS, PPS are mandatory
		return nullptr;
	}

	ov::BitWriter bit(1024);

	bit.WriteBits(8, Version()); // configurationVersion
	bit.WriteBits(2, GeneralProfileSpace());
	bit.WriteBits(1, GeneralTierFlag());
	bit.WriteBits(5, GeneralProfileIDC());
	bit.WriteBits(32, GeneralProfileCompatibilityFlags());

	bit.WriteBits(32, GeneralConstraintIndicatorFlags() >> 16);
	bit.WriteBits(16, GeneralConstraintIndicatorFlags() & 0xFFFF);

	bit.WriteBits(8, GeneralLevelIDC());

	bit.WriteBits(4, 0b1111); // reserved
	bit.WriteBits(12, MinSpatialSegmentationIDC());
	bit.WriteBits(6, 0b111111); // reserved
	bit.WriteBits(2, ParallelismType());
	bit.WriteBits(6, 0b111111); // reserved
	bit.WriteBits(2, ChromaFormat());
	bit.WriteBits(5, 0b11111); // reserved
	bit.WriteBits(3, BitDepthLumaMinus8());
	bit.WriteBits(5, 0b11111); // reserved
	bit.WriteBits(3, BitDepthChromaMinus8());
	bit.WriteBits(16, AvgFrameRate());
	bit.WriteBits(2, ConstantFrameRate());
	bit.WriteBits(3, NumTemporalLayers());
	bit.WriteBits(1, TemporalIdNested());
	bit.WriteBits(2, LengthSizeMinusOne());

	bit.WriteBits(8, _nal_units.size()); // numOfArrays

	for (const auto &[nal_type, nal_units] : _nal_units)
	{
		// array_completeness when equal to 1 indicates that all NAL units of the given type are in the following array and none are in the stream; when equal to 0 indicates that additional NAL units of the indicated type may be in the stream; the default and permitted values are constrained by the sample entry name;
		bit.WriteBits(1, 1); // array_completeness

		bit.WriteBits(1, 0); // reserved
		bit.WriteBits(6, nal_type); // nal_unit_type
		bit.WriteBits(16, nal_units.size()); // numNalus

		for (auto &nal_unit : nal_units)
		{
			bit.WriteBits(16, nal_unit->GetLength()); // nalUnitLength
			if (bit.WriteData(nal_unit->GetDataAs<uint8_t>(), nal_unit->GetLength()) == false) // nalUnit
			{
				return nullptr;
			}
		}
	}

	return bit.GetDataObject();
}

void HEVCDecoderConfigurationRecord::AddNalUnit(H265NALUnitType nal_type, const std::shared_ptr<ov::Data> &nal_unit)
{
	auto &v = _nal_units[static_cast<uint8_t>(nal_type)];
	v.push_back(nal_unit);
	
	if (nal_type == H265NALUnitType::SPS)
	{
		// Set Info from SPS
		if (H265Parser::ParseSPS(nal_unit->GetDataAs<uint8_t>(), nal_unit->GetLength(), _sps))
		{
			auto profile_tier_level = _sps.GetProfileTierLevel();
			_general_profile_space = profile_tier_level._general_profile_space;
			_general_tier_flag = profile_tier_level._general_tier_flag;
			_general_profile_idc = profile_tier_level._general_profile_idc;
			_general_profile_compatibility_flags = profile_tier_level._general_profile_compatibility_flags;
			_general_constraint_indicator_flags = profile_tier_level._general_constraint_indicator_flags;
			_general_level_idc = profile_tier_level._general_level_idc;

			_chroma_format = _sps.GetChromaFormatIdc();
			_bit_depth_chroma_minus8 = _sps.GetBitDepthChromaMinus8();
			_bit_depth_luma_minus8 = _sps.GetBitDepthLumaMinus8();

			auto vui_parameters = _sps.GetVuiParameters();
			_min_spatial_segmentation_idc = vui_parameters._min_spatial_segmentation_idc;

			// TODO(Getroot) : _num_temporal_layers must be the largest value among max_sub_layers_minus1 of VPS and max_sub_layers_minus1 of SPS.
			_num_temporal_layers = std::max<uint8_t>(_num_temporal_layers, _sps.GetMaxSubLayersMinus1() + 1);
			_temporal_id_nested = _sps.GetTemporalIdNestingFlag();
			_length_size_minus_one = 3;

			_avg_frame_rate = 0;
			_constant_frame_rate = 0;
		}
		else
		{
			logte("Failed to parse SPS");
		}
	}
	else if (nal_type == H265NALUnitType::PPS)
	{
		// TODO(Getroot) : Implement PPS parser for getting following values
		// _parallelism_type can be derived from the following PPS values:
		// if entropy coding sync enabled flag(1) && tiles enabled flag(1)
		// 		parallelism_type = 0 // mixed type parallel decoding
		// else if entropy coding sync enabled flag(1)
		// 		parallelism_type = 3 // wavefront-based parallel decoding
		// else if tiles enabled flag(1)
		// 		parallelism_type = 2 // tile-based parallel decoding
		// else 
		// 		parallelism_type = 1 // slice-based parallel decoding
		_parallelism_type = 0;
	}
	else if (nal_type == H265NALUnitType::VPS)
	{
		//_num_temporal_layers = std::max<uint8_t>(_num_temporal_layers, vps.GetMaxSubLayersMinus1() + 1);
	}
}

uint8_t HEVCDecoderConfigurationRecord::Version()
{
	return _version;
}
uint8_t	HEVCDecoderConfigurationRecord::GeneralProfileSpace()
{
	return _general_profile_space;
}
uint8_t HEVCDecoderConfigurationRecord::GeneralTierFlag()
{
	return _general_tier_flag;
}
uint8_t HEVCDecoderConfigurationRecord::GeneralProfileIDC()
{
	return _general_profile_idc;
}
uint32_t HEVCDecoderConfigurationRecord::GeneralProfileCompatibilityFlags()
{
	return _general_profile_compatibility_flags;
}
uint64_t HEVCDecoderConfigurationRecord::GeneralConstraintIndicatorFlags()
{
	return _general_constraint_indicator_flags;
}
uint8_t HEVCDecoderConfigurationRecord::GeneralLevelIDC()
{
	return _general_level_idc;
}
uint16_t HEVCDecoderConfigurationRecord::MinSpatialSegmentationIDC()
{
	return _min_spatial_segmentation_idc;
}
uint8_t HEVCDecoderConfigurationRecord::ParallelismType()
{
	return _parallelism_type;
}
uint8_t HEVCDecoderConfigurationRecord::ChromaFormat()
{
	return _chroma_format;
}
uint8_t HEVCDecoderConfigurationRecord::BitDepthLumaMinus8()
{
	return _bit_depth_luma_minus8;
}
uint8_t HEVCDecoderConfigurationRecord::BitDepthChromaMinus8()
{
	return _bit_depth_chroma_minus8;
}
uint16_t HEVCDecoderConfigurationRecord::AvgFrameRate()
{
	return _avg_frame_rate;
}
uint8_t HEVCDecoderConfigurationRecord::ConstantFrameRate()
{
	return _constant_frame_rate;
}
uint8_t HEVCDecoderConfigurationRecord::NumTemporalLayers()
{
	return _num_temporal_layers;
}
uint8_t HEVCDecoderConfigurationRecord::TemporalIdNested()
{
	return _temporal_id_nested;
}
uint8_t HEVCDecoderConfigurationRecord::LengthSizeMinusOne()
{
	// lengthSizeMinusOne plus 1 indicates the length in bytes of the NALUnitLength field in an HEVC video sample in the stream to which this configuration record applies. For example, a size of one byte is indicated with a value of 0. The value of this field shall be one of 0, 1, or 3 corresponding to a length encoded with 1, 2, or 4 bytes, respectively.

	return _length_size_minus_one;
}
uint8_t HEVCDecoderConfigurationRecord::NumOfArrays()
{
	return _nal_units.size();
}
std::vector<std::shared_ptr<ov::Data>> HEVCDecoderConfigurationRecord::GetNalUnits(H265NALUnitType nal_type)
{
	if (_nal_units.find(static_cast<uint8_t>(nal_type)) == _nal_units.end())
	{
		return std::vector<std::shared_ptr<ov::Data>>();
	}

	return _nal_units[static_cast<uint8_t>(nal_type)];
}