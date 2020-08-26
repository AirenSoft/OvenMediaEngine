#include "h265_sps.h"
#include "h265_types.h"

bool H265Sps::Parse(const uint8_t *sps_bitstream, size_t length, H265Sps &sps)
{
	NalUnitBitstreamParser parser(sps_bitstream, length);

	///////////////
	// Header
	///////////////

	// forbidden_zero_bit
	parser.Skip(1);

	// type
	uint8_t nal_type;
	if(parser.ReadBits(6, nal_type) == false)
	{
		return false;
	}
	
	if(nal_type != static_cast<uint8_t>(H265NALUnitType::SPS))
	{
		return false;
	}

	uint8_t layer_id;
	if(parser.ReadBits(6, layer_id) == false)
	{
		return false;
	}

	uint8_t temporal_id_plus1;
	if(parser.ReadBits(3, temporal_id_plus1) == false)
	{
		return false;
	}

	// SPS
	uint8_t sps_video_parameter_set_id;
	if(parser.ReadBits(4, sps_video_parameter_set_id) == false)
	{
		return false;
	}

	uint8_t sps_max_sub_layers_minus1;
	if(parser.ReadBits(3, sps_max_sub_layers_minus1) == false)
	{
		return false;
	}

	uint8_t sps_temporal_id_nesting_flag;
	if(parser.ReadBits(1, sps_temporal_id_nesting_flag) == false)
	{
		return false;
	}

	// profile tier level
	uint8_t general_profile_space;
	if(parser.ReadBits(2, general_profile_space) == false)
	{
		return false;
	}

	uint8_t general_tier_flag;
	if(parser.ReadBits(1, general_tier_flag ) == false)
	{
		return false;
	}

	uint8_t general_profile_idc;
	if(parser.ReadBits(5, general_profile_idc) == false)
	{
		return false;
	}

	// general_profile_compatibility_flag
	if(parser.Skip(32) == false)
	{
		return false;
	}

	uint8_t general_progressive_source_flag;
	if(parser.ReadBits(1, general_progressive_source_flag) == false)
	{
		return false;
	}

	uint8_t general_interlaced_source_flag;
	if(parser.ReadBits(1, general_interlaced_source_flag) == false)
	{
		return false;
	}

	uint8_t general_non_packed_constraint_flag;
	if(parser.ReadBits(1, general_non_packed_constraint_flag) == false)
	{
		return false;
	}

	uint8_t general_frame_only_constraint_flag;
	if(parser.ReadBits(1, general_frame_only_constraint_flag) == false)
	{
		return false;
	}

	if(parser.Skip(32) == false)
	{
		return false;
	}

	if(parser.Skip(12) == false)
	{
		return false;
	}

	uint8_t general_level_idc;
	if(parser.ReadBits(8, general_level_idc) == false)
	{
		return false;
	}

	

	return true;
}

unsigned int H265Sps::GetWidth() const
{
	return _width;
}
unsigned int H265Sps::GetHeight() const
{
	return _height;
}
uint8_t H265Sps::GetProfile() const
{
	return _profile;
}
uint8_t H265Sps::GetCodecLevel() const
{
	return _codec_level;
}
unsigned int H265Sps::GetFps() const
{
	return _fps;
}
unsigned int H265Sps::GetId() const
{
	return _id;
}
unsigned int H265Sps::GetMaxNrOfReferenceFrames() const
{
	return _max_nr_of_reference_frames;
}

ov::String H265Sps::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[H264Sps]\n");

	out_str.AppendFormat("\tProfile(%d)\n", GetProfile());
	out_str.AppendFormat("\tCodecLevel(%d)\n", GetCodecLevel());
	out_str.AppendFormat("\tWidth(%d)\n", GetWidth());
	out_str.AppendFormat("\tHeight(%d)\n", GetHeight());
	out_str.AppendFormat("\tFps(%d)\n", GetFps());
	out_str.AppendFormat("\tId(%d)\n", GetId());
	out_str.AppendFormat("\tMaxNrOfReferenceFrames(%d)\n", GetMaxNrOfReferenceFrames());
	out_str.AppendFormat("\tAspectRatio(%d:%d)\n", _aspect_ratio._width, _aspect_ratio._height);

	return out_str;
}
