#include "h264_parser.h"

bool H264Parser::CheckKeyframe(const uint8_t *bitstream, size_t length)
{
    size_t offset = 0;
    while(offset < length)
    {
        size_t remaining = length - offset;
        const uint8_t* data = bitstream + offset;

        if((remaining >= 3 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) || 
            (remaining >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01))
        {
            if(data[2] == 0x01)
            {
                offset += 3;
            }
            else
            {
                offset += 4;
            }

			if(length - offset > H264_NAL_UNIT_HEADER_SIZE)
			{
				H264NalUnitHeader header;
				ParseNalUnitHeader(bitstream+offset, H264_NAL_UNIT_HEADER_SIZE, header);

				if(header.GetNalUnitType() == H264NalUnitType::IdrSlice ||
				header.GetNalUnitType() == H264NalUnitType::Sps)
				{
					return true;
				}
			}
		}
		else
		{
			offset ++;
		}
	}
	return false;
}

bool H264Parser::ParseNalUnitHeader(const uint8_t *nalu, size_t length, H264NalUnitHeader &header)
{
    NalUnitBitstreamParser parser(nalu, length);

	if(length < H264_NAL_UNIT_HEADER_SIZE)
	{
		return false;
	}

    return ParseNalUnitHeader(parser, header);
}

bool H264Parser::ParseNalUnitHeader(NalUnitBitstreamParser &parser, H264NalUnitHeader &header)
{
    // forbidden_zero_bit
    uint8_t forbidden_zero_bit;
	if(parser.ReadBits(1, forbidden_zero_bit) == false)
    {
        return false;
    }

    if(forbidden_zero_bit != 0)
    {
        return false;
    }

    uint8_t nal_ref_idc;
    if(parser.ReadBits(2, nal_ref_idc) == false)
    {
        return false;
    }
    header._nal_ref_idc = nal_ref_idc;

    uint8_t nal_type;
    if(parser.ReadBits(5, nal_type) == false)
    {
        return false;
    }

    header._type = static_cast<H264NalUnitType>(nal_type);

    return true;
}

bool H264Parser::ParseSPS(const uint8_t *nalu, size_t length, H264SPS &sps)
{
    NalUnitBitstreamParser parser(nalu, length);

	H264NalUnitHeader header;
	if(ParseNalUnitHeader(parser, header) == false)
	{
		return false;
	}

	if(header.GetNalUnitType() != H264NalUnitType::Sps)
	{
		return false;
	}

	if(!parser.ReadU8(sps._profile))
	{
		return false;
	}

	// Contraint set (5bits) and 3 reserved zero bits
	if(!parser.ReadU8(sps._constraint))
	{
		return false;
	}

	if(!parser.ReadU8(sps._codec_level)) 
	{
		return false;
	}

	if(!parser.ReadUEV(sps._id)) 
	{
		return false;
	}

	if(sps._profile == 44 || sps._profile == 83 || sps._profile == 86 || sps._profile == 100 ||
		sps._profile == 110 || sps._profile == 118 || sps._profile == 122 || sps._profile == 244)
	{
		uint32_t chroma_format;
		if (!parser.ReadUEV(chroma_format))
		{
			return false;
		}

		if (chroma_format == 3)
		{
			if(!parser.Skip(1)) 
			{
				return false;
			}
		}

		uint32_t luma_bit_depth;
		if(!parser.ReadUEV(luma_bit_depth)) 
		{
			return false;
		}
		luma_bit_depth += 8;

		uint32_t chroma_bit_depth;
		if(!parser.ReadUEV(chroma_bit_depth))
		{
			return false;
		}
		chroma_bit_depth += 8;

		uint8_t zero_transform_bypass_flag;
		if(!parser.ReadBit(zero_transform_bypass_flag)) 
		{
			return false;
		}

		uint8_t scaling_matrix_present_flag;
		if(!parser.ReadBit(scaling_matrix_present_flag))
		{
			return false;
		}

		if(scaling_matrix_present_flag)
		{
			const size_t matrix_size = chroma_format == 3 ? 12 : 8;
			for(size_t index = 0; index < matrix_size; ++index)
			{
				uint8_t scaling_list_present_flag;
				if(!parser.ReadBit(scaling_list_present_flag))
				{
					return false;
				}

				if(scaling_list_present_flag)
				{
					// TODO: add support for scaling list
					return false;
				}
			}
		}
	}

	if(uint32_t log2_max_frame_num_minus4; !parser.ReadUEV(log2_max_frame_num_minus4))
	{
		return false;
	}

	uint32_t order_type;
	if(!parser.ReadUEV(order_type)) 
	{
		return false;
	}

	if(order_type == 0)
	{
		if(uint32_t log2_max_pic_order_cnt_lsb_minus4; !parser.ReadUEV(log2_max_pic_order_cnt_lsb_minus4)) 
		{
			return false;
		}
	}
	else if(order_type == 1)
	{	
		uint8_t delta_pic_order_always_zero_flag; 
		if(!parser.ReadBit(delta_pic_order_always_zero_flag))
		{
			return false;
		}

		int32_t offset_for_non_ref_pic; 
		if(!parser.ReadSEV(offset_for_non_ref_pic))
		{
			return false;
		}

		int32_t offset_for_top_to_bottom_field; 
		if(!parser.ReadSEV(offset_for_top_to_bottom_field))
		{
			return false;
		}

		uint32_t num_ref_frames_in_pic_order_cnt_cycle; 
		if(!parser.ReadUEV(num_ref_frames_in_pic_order_cnt_cycle)) 
		{
			return false;
		}

		for(uint32_t index = 0; index < num_ref_frames_in_pic_order_cnt_cycle; ++index)
		{
			int32_t reference_frame_offset;
			if (!parser.ReadSEV(reference_frame_offset)) 
			{
				return false;
			}
		}
	}

	if(!parser.ReadUEV(sps._max_nr_of_reference_frames))
	{
		return false;
	}
	
	uint8_t gaps_in_frame_num_value_allowed_flag;
	if(!parser.ReadBit(gaps_in_frame_num_value_allowed_flag)) 
	{
		return false;
	}

	{
		// everything needed to determine the size lives in this block
		uint32_t pic_width_in_mbs_minus1;
		if(!parser.ReadUEV(pic_width_in_mbs_minus1))
		{
			return false;
		}

		uint32_t pic_height_in_map_units_minus1;
		if(!parser.ReadUEV(pic_height_in_map_units_minus1))
		{
			return false;
		}

		uint8_t frame_mbs_only_flag;
		if(!parser.ReadBit(frame_mbs_only_flag))
		{
			return false;
		}

		if(!frame_mbs_only_flag)
		{
			uint8_t mb_adaptive_frame_field_flag;
			if(!parser.ReadBit(mb_adaptive_frame_field_flag))
			{
				return false;
			}
		}

		uint8_t direct_8x8_inference_flag; 
		if(!parser.ReadBit(direct_8x8_inference_flag))
		{
			return false;
		}

		uint8_t frame_cropping_flag;
		if(!parser.ReadBit(frame_cropping_flag))
		{
			return false;
		}

		uint32_t crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;
		if(frame_cropping_flag)
		{
			if(!parser.ReadUEV(crop_left))
			{
				return false;
			}

			if(!parser.ReadUEV(crop_right))
			{
				return false;
			}

			if(!parser.ReadUEV(crop_top))
			{
				return false;
			}

			if(!parser.ReadUEV(crop_bottom)) 
			{
				return false;
			}
		}

		sps._width = (pic_width_in_mbs_minus1 + 1) * 16 - 2 * crop_left - 2 * crop_right;
		sps._height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16) - 2 * crop_top - 2 * crop_bottom;
	}


	uint8_t vui_parameters_present_flag;
	if(!parser.ReadBit(vui_parameters_present_flag))
	{
		return false;
	}

	if(vui_parameters_present_flag)
	{
		uint8_t aspect_ratio_info_present_flag;
		if(!parser.ReadBit(aspect_ratio_info_present_flag))
		{
			return false;
		}

		if(aspect_ratio_info_present_flag)
		{
			uint8_t aspect_ratio_idc;

			if (!parser.ReadU8(aspect_ratio_idc)) 
			{
				return false;
			}

            sps._aspect_ratio_idc = aspect_ratio_idc;

			if(aspect_ratio_idc == 255)
			{
				if(parser.ReadU16(sps._aspect_ratio._width) == false ||
					parser.ReadU16(sps._aspect_ratio._height) == false)
				{
					return false;
				}
			}
			else
			{
				// TODO: set the aspect ratio from the table mapping aspect_ratio_idc to actual values from the 
				// H.264 spec
			}
		}

		{
			uint8_t overscan_info_present_flag;
			if(!parser.ReadBit(overscan_info_present_flag))
			{
				return false;
			}
			if(overscan_info_present_flag)
			{
				if(uint8_t overscan_appropriate_flag; !parser.ReadBit(overscan_appropriate_flag)) 
				{
					return false;
				}
			}
		}

		{
			uint8_t video_signal_type_present_flag;
			if(!parser.ReadBit(video_signal_type_present_flag))
			{
				return false;
			}

			if(video_signal_type_present_flag)
			{
				{
					uint8_t video_format = 0, bit;
					if (!(parser.ReadBit(bit) &&
						(video_format |= bit, video_format <<= 1, parser.ReadBit(bit)) &&
						(video_format |= bit, video_format <<= 1, parser.ReadBit(bit))))
					{
						return false;
					}
					video_format |= bit;
				}

				if(uint8_t video_full_range_flag; !parser.ReadBit(video_full_range_flag))
				{
					return false;
				}

				uint8_t colour_description_present_flag;
				if(!parser.ReadBit(colour_description_present_flag))
				{
					return false;
				}

				if(colour_description_present_flag)
				{
					if (uint8_t colour_primaries; !parser.ReadU8(colour_primaries))
					{
						return false;
					}
					
					if(uint8_t transfer_characteristics; !parser.ReadU8(transfer_characteristics))
					{
						return false;
					}
					
					if(uint8_t matrix_coefficients; !parser.ReadU8(matrix_coefficients))
					{
						return false;
					}
				}
			}
		}

		{
			uint8_t chroma_loc_info_present_flag;
			if(!parser.ReadBit(chroma_loc_info_present_flag))
			{
				return false;
			}

			if(chroma_loc_info_present_flag)
			{	
				uint32_t chroma_sample_loc_type_top_field; 
				if (!parser.ReadUEV(chroma_sample_loc_type_top_field))
				{
					return false;
				}
				uint32_t chroma_sample_loc_type_bottom_field; 
				if(!parser.ReadUEV(chroma_sample_loc_type_bottom_field))
				{
					return false;
				}
			}
		}

		{
			uint8_t timing_info_present_flag;
			if(!parser.ReadBit(timing_info_present_flag))
			{
				return false;
			}
			
			if(timing_info_present_flag)
			{
				uint32_t num_units_in_tick, time_scale;
				uint8_t fixed_frame_rate_flag;


				if(!(parser.ReadUEV(num_units_in_tick) && parser.ReadUEV(time_scale) && parser.ReadBit(fixed_frame_rate_flag))) 
				{
				 	return false;
				}
				//loge("H264Sps", "fixed_frame_rate_flag(%d) time_scale(%d), num_units_in_tick(%d)", (uint32_t)fixed_frame_rate_flag, (uint32_t)time_scale, (uint32_t)num_units_in_tick);

				if(fixed_frame_rate_flag)
				{
					sps._fps = time_scale / (2 * num_units_in_tick);
				}
			}
		}
		// Currently skip the remaining part of VUI parameters
	}

	return true;
}