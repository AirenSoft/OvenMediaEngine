#include "h264_sps.h"
#include "h264.h"

bool H264Sps::Parse(const uint8_t *sps_bitstream, size_t length, H264Sps &sps)
{
    // all the variables here are scoped to minimum needed visibility (the c++17 if initializer kicks in nicely
    // for this use case) to avoid logic errors

    H264NalUnitBitstreamParser sps_bitstream_parser(sps_bitstream, length);
	uint8_t nal_type;

    if(sps_bitstream_parser.ReadU8(nal_type) == false)
	{
		return false;
	}
	
	if((nal_type & kH264NalUnitTypeMask) != H264NalUnitType::Sps) 
	{
		return false;
	}

    if(!sps_bitstream_parser.ReadU8(sps._profile)) 
	{
		return false;
	}
        
    // Skip constraint set (5 bits) and 3 reserved zero bits
	uint8_t dummy; 
    if(!sps_bitstream_parser.ReadU8(dummy))
	{
		return false;
	}

    if(!sps_bitstream_parser.ReadU8(sps._codec_level)) 
	{
		return false;
	}

    if(!sps_bitstream_parser.ReadUEV(sps._id)) 
	{
		return false;
	}

    if(sps._profile == 44 || sps._profile == 83 || sps._profile == 86 || sps._profile == 100 ||
        sps._profile == 110 || sps._profile == 118 || sps._profile == 122 || sps._profile == 244)
    {
        uint32_t chroma_format;
        if (!sps_bitstream_parser.ReadUEV(chroma_format))
		{
			return false;
		}

        if (chroma_format == 3)
        {
            if(!sps_bitstream_parser.Skip(1)) 
			{
				return false;
			}
        }

		uint32_t luma_bit_depth;
		if(!sps_bitstream_parser.ReadUEV(luma_bit_depth)) 
		{
			return false;
		}
		luma_bit_depth += 8;

		uint32_t chroma_bit_depth;
		if(!sps_bitstream_parser.ReadUEV(chroma_bit_depth))
		{
			return false;
		}
		chroma_bit_depth += 8;

		uint8_t zero_transform_bypass_flag;
        if(!sps_bitstream_parser.ReadBit(zero_transform_bypass_flag)) 
		{
			return false;
		}

        uint8_t scaling_matrix_present_flag;
        if(!sps_bitstream_parser.ReadBit(scaling_matrix_present_flag))
		{
			return false;
		}

        if(scaling_matrix_present_flag)
        {
            const size_t matrix_size = chroma_format == 3 ? 12 : 8;
            for(size_t index = 0; index < matrix_size; ++index)
            {
                uint8_t scaling_list_present_flag;
                if(!sps_bitstream_parser.ReadBit(scaling_list_present_flag))
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

    if(uint32_t log2_max_frame_num_minus4; !sps_bitstream_parser.ReadUEV(log2_max_frame_num_minus4))
	{
		return false;
	}

    uint32_t order_type;
    if(!sps_bitstream_parser.ReadUEV(order_type)) 
	{
		return false;
	}

    if(order_type == 0)
    {
        if(uint32_t log2_max_pic_order_cnt_lsb_minus4; !sps_bitstream_parser.ReadUEV(log2_max_pic_order_cnt_lsb_minus4)) 
		{
			return false;
		}
    }
    else if(order_type == 1)
    {	
		uint8_t delta_pic_order_always_zero_flag; 
        if(!sps_bitstream_parser.ReadBit(delta_pic_order_always_zero_flag))
		{
			return false;
		}

		int32_t offset_for_non_ref_pic; 
        if(!sps_bitstream_parser.ReadSEV(offset_for_non_ref_pic))
		{
			return false;
		}

		int32_t offset_for_top_to_bottom_field; 
        if(!sps_bitstream_parser.ReadSEV(offset_for_top_to_bottom_field))
		{
			return false;
		}

        uint32_t num_ref_frames_in_pic_order_cnt_cycle; 
        if(!sps_bitstream_parser.ReadUEV(num_ref_frames_in_pic_order_cnt_cycle)) 
		{
			return false;
		}

        for(uint32_t index = 0; index < num_ref_frames_in_pic_order_cnt_cycle; ++index)
        {
            int32_t reference_frame_offset;
            if (!sps_bitstream_parser.ReadSEV(reference_frame_offset)) 
			{
				return false;
			}
        }
    }

    if(!sps_bitstream_parser.ReadUEV(sps._max_nr_of_reference_frames))
	{
		return false;
	}
	
	uint8_t gaps_in_frame_num_value_allowed_flag;
    if(!sps_bitstream_parser.ReadBit(gaps_in_frame_num_value_allowed_flag)) 
	{
		return false;
	}

    {
        // everything needed to determine the size lives in this block
        uint32_t pic_width_in_mbs_minus1;
        if(!sps_bitstream_parser.ReadUEV(pic_width_in_mbs_minus1))
		{
			return false;
		}

        uint32_t pic_height_in_map_units_minus1;
        if(!sps_bitstream_parser.ReadUEV(pic_height_in_map_units_minus1))
		{
			return false;
		}

        uint8_t frame_mbs_only_flag;
        if(!sps_bitstream_parser.ReadBit(frame_mbs_only_flag))
		{
			return false;
		}

        if(!frame_mbs_only_flag)
        {
            uint8_t mb_adaptive_frame_field_flag;
            if(!sps_bitstream_parser.ReadBit(mb_adaptive_frame_field_flag))
			{
				return false;
			}
        }

		uint8_t direct_8x8_inference_flag; 
        if(!sps_bitstream_parser.ReadBit(direct_8x8_inference_flag))
		{
			return false;
		}

        uint8_t frame_cropping_flag;
        if(!sps_bitstream_parser.ReadBit(frame_cropping_flag))
		{
			return false;
		}

        uint32_t crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;
        if(frame_cropping_flag)
        {
            if(!sps_bitstream_parser.ReadUEV(crop_left))
			{
				return false;
			}

            if(!sps_bitstream_parser.ReadUEV(crop_right))
			{
				return false;
			}

            if(!sps_bitstream_parser.ReadUEV(crop_top))
			{
				return false;
			}

            if(!sps_bitstream_parser.ReadUEV(crop_bottom)) 
			{
				return false;
			}
        }

        sps._width = (pic_width_in_mbs_minus1 + 1) * 16 - 2 * crop_left - 2 * crop_right;
        sps._height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16) - 2 * crop_top - 2 * crop_bottom;
    }


    uint8_t vui_parameters_present_flag;
    if(!sps_bitstream_parser.ReadBit(vui_parameters_present_flag))
	{
		return false;
	}

    if(vui_parameters_present_flag)
    {
        uint8_t aspect_ratio_info_present_flag;
        if(!sps_bitstream_parser.ReadBit(aspect_ratio_info_present_flag))
		{
			return false;
		}

        if(aspect_ratio_info_present_flag)
        {
            uint8_t aspect_ratio_idc;
            if (!sps_bitstream_parser.ReadU8(aspect_ratio_idc)) 
			{
				return false;
			}

            if(aspect_ratio_idc == 255)
            {
                if(sps_bitstream_parser.ReadU16(sps._aspect_ratio._width) == false ||
                    sps_bitstream_parser.ReadU16(sps._aspect_ratio._height) == false)
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
            if(!sps_bitstream_parser.ReadBit(overscan_info_present_flag))
			{
				return false;
			}
            if(overscan_info_present_flag)
            {
                if(uint8_t overscan_appropriate_flag; !sps_bitstream_parser.ReadBit(overscan_appropriate_flag)) 
				{
					return false;
				}
            }
        }

        {
            uint8_t video_signal_type_present_flag;
            if(!sps_bitstream_parser.ReadBit(video_signal_type_present_flag))
			{
				return false;
			}

            if(video_signal_type_present_flag)
            {
                {
                    uint8_t video_format = 0, bit;
                    if (!(sps_bitstream_parser.ReadBit(bit) &&
                        (video_format |= bit, video_format <<= 1, sps_bitstream_parser.ReadBit(bit)) &&
                        (video_format |= bit, video_format <<= 1, sps_bitstream_parser.ReadBit(bit))))
                    {
                        return false;
                    }
                    video_format |= bit;
                }

                if(uint8_t video_full_range_flag; !sps_bitstream_parser.ReadBit(video_full_range_flag))
				{
					return false;
				}

                uint8_t colour_description_present_flag;
                if(!sps_bitstream_parser.ReadBit(colour_description_present_flag))
				{
					return false;
				}

                if(colour_description_present_flag)
                {
                    if (uint8_t colour_primaries; !sps_bitstream_parser.ReadU8(colour_primaries))
					{
						return false;
					}
                    
					if(uint8_t transfer_characteristics; !sps_bitstream_parser.ReadU8(transfer_characteristics))
					{
						return false;
					}
                    
					if(uint8_t matrix_coefficients; !sps_bitstream_parser.ReadU8(matrix_coefficients))
					{
						return false;
					}
                }
            }
        }

        {
            uint8_t chroma_loc_info_present_flag;
            if(!sps_bitstream_parser.ReadBit(chroma_loc_info_present_flag))
			{
				return false;
			}

            if(chroma_loc_info_present_flag)
            {	
				uint32_t chroma_sample_loc_type_top_field; 
                if (!sps_bitstream_parser.ReadUEV(chroma_sample_loc_type_top_field))
				{
					return false;
				}
				uint32_t chroma_sample_loc_type_bottom_field; 
                if(!sps_bitstream_parser.ReadUEV(chroma_sample_loc_type_bottom_field))
				{
					return false;
				}
            }
        }

        {
            uint8_t timing_info_present_flag;
            if(!sps_bitstream_parser.ReadBit(timing_info_present_flag))
			{
				return false;
			}
			
            if(timing_info_present_flag)
            {
                uint32_t num_units_in_tick, time_scale;
                uint8_t fixed_frame_rate_flag;
                if(!(sps_bitstream_parser.ReadUEV(num_units_in_tick) && sps_bitstream_parser.ReadUEV(time_scale) && sps_bitstream_parser.ReadBit(fixed_frame_rate_flag))) 
				{
					return false;
				}

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

unsigned int H264Sps::GetWidth() const
{
    return _width;
}
unsigned int H264Sps::GetHeight() const
{
    return _height;
}
uint8_t H264Sps::GetProfile() const
{
    return _profile;
}
uint8_t H264Sps::GetCodecLevel() const
{
    return _codec_level;
}
unsigned int H264Sps::GetFps() const
{
    return _fps;
}
unsigned int H264Sps::GetId() const
{
    return _id;
}
unsigned int H264Sps::GetMaxNrOfReferenceFrames() const
{
    return _max_nr_of_reference_frames;
}
