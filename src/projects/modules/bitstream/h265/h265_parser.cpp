// I developed the code of this file by referring to the source code of virinext's hevcsbrowser (https://github.com/virinext/hevcesbrowser). 
// Thanks to virinext.
// - Getroot

#include "h265_parser.h"
#include "h265_types.h"

// returns offset (start point), code_size : 3(001) or 4(0001)
// returns -1 if there is no start code in the buffer
int H265Parser::FindAnnexBStartCode(const uint8_t *bitstream, size_t length, size_t &start_code_size)
{
	size_t offset = 0;
	start_code_size = 0;
	
	while(offset < length)
	{
		size_t remaining = length - offset;
		const uint8_t* data = bitstream + offset;

		if((remaining >= 3 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) || 
            (remaining >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01))
        {
            if(data[2] == 0x01)
            {
                start_code_size = 3;
            }
            else
            {
                start_code_size = 4;
            }

			return offset;
		}
		else
		{
			offset += 1;
		}
	}

	return -1;
}

bool H265Parser::CheckKeyframe(const uint8_t *bitstream, size_t length)
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

			if(length - offset > H265_NAL_UNIT_HEADER_SIZE)
			{
				H265NalUnitHeader header;
				ParseNalUnitHeader(bitstream+offset, H265_NAL_UNIT_HEADER_SIZE, header);

				if(header.GetNalUnitType() == H265NALUnitType::IDR_W_RADL ||
				header.GetNalUnitType() == H265NALUnitType::CRA_NUT ||
				header.GetNalUnitType() == H265NALUnitType::BLA_W_RADL) 
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

bool H265Parser::ParseNalUnitHeader(const uint8_t *nalu, size_t length, H265NalUnitHeader &header)
{
	NalUnitBitstreamParser parser(nalu, length);

	if(length < H265_NAL_UNIT_HEADER_SIZE)
	{
		return false;
	}

	return ParseNalUnitHeader(parser, header);
}

bool H265Parser::ParseNalUnitHeader(NalUnitBitstreamParser &parser, H265NalUnitHeader &header)
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

	// type
	uint8_t nal_type;
	if(parser.ReadBits(6, nal_type) == false)
	{
		return false;
	}

	header._type = static_cast<H265NALUnitType>(nal_type);

	uint8_t layer_id;
	if(parser.ReadBits(6, layer_id) == false)
	{
		return false;
	}

	header._layer_id = layer_id;

	uint8_t temporal_id_plus1;
	if(parser.ReadBits(3, temporal_id_plus1) == false)
	{
		return false;
	}

	header._temporal_id_plus1 = temporal_id_plus1;
	return true;
}

bool H265Parser::ParseSPS(const uint8_t *nalu, size_t length, H265SPS &sps)
{
	NalUnitBitstreamParser parser(nalu, length);

	H265NalUnitHeader header;

	if(ParseNalUnitHeader(parser, header) == false)
	{
		return false;
	}

	if(header.GetNalUnitType() != H265NALUnitType::SPS)
	{
		return false;
	}

	///////////////////////
	// SPS
	///////////////////////
	uint8_t sps_video_parameter_set_id;
	if(parser.ReadBits(4, sps_video_parameter_set_id) == false)
	{
		return false;
	}

	uint8_t max_sub_layers_minus1;
	if(parser.ReadBits(3, max_sub_layers_minus1) == false)
	{
		return false;
	}

	uint8_t temporal_id_nesting_flag;
	if(parser.ReadBits(1, temporal_id_nesting_flag) == false)
	{
		return false;
	}

	ProfileTierLevel profile;
	if(ProcessProfileTierLevel(max_sub_layers_minus1, parser, profile) == false)
	{
		return false;
	}

	sps._profile_tier_level = profile;
	
	uint32_t sps_seq_parameter_set_id;
	if(parser.ReadUEV(sps_seq_parameter_set_id) == false)
	{
		return false;
	}

	uint32_t chroma_format_idc;
	if(parser.ReadUEV(chroma_format_idc) == false)
	{
		return false;
	}

	uint8_t separate_colour_plane_flag = 0;
	if(chroma_format_idc == 3)
	{
		if(parser.ReadBits(1, separate_colour_plane_flag) == false)
		{
			return false;
		}
	}

	uint32_t pic_width_in_luma_samples;
	if(parser.ReadUEV(pic_width_in_luma_samples) == false)
	{
		return false;
	}

	sps._width = pic_width_in_luma_samples;

	uint32_t pic_height_in_luma_samples;
	if(parser.ReadUEV(pic_height_in_luma_samples) == false)
	{
		return false;
	}	

	sps._height = pic_height_in_luma_samples;

	uint8_t conformance_window_flag;
	if(parser.ReadBits(1, conformance_window_flag) == false)
	{
		return false;
	}

	uint32_t conf_win_left_offset, conf_win_right_offset, conf_win_top_offset, conf_win_bottom_offset;
	if(conformance_window_flag)
	{
		if(parser.ReadUEV(conf_win_left_offset) == false)
		{
			return false;
		}

		if(parser.ReadUEV(conf_win_right_offset) == false)
		{
			return false;
		}

		if(parser.ReadUEV(conf_win_top_offset) == false)
		{
			return false;
		}

		if(parser.ReadUEV(conf_win_bottom_offset) == false)
		{
			return false;
		}
	}

	uint32_t bit_depth_luma_minus8;
	if(parser.ReadUEV(bit_depth_luma_minus8) == false)
	{
		return false;
	}

	uint32_t bit_depth_chroma_minus8;
	if(parser.ReadUEV(bit_depth_chroma_minus8) == false)
	{
		return false;
	}

	uint32_t log2_max_pic_order_cnt_lsb_minus4;
	if(parser.ReadUEV(log2_max_pic_order_cnt_lsb_minus4) == false)
	{
		return false;
	}

	uint8_t sps_sub_layer_ordering_info_present_flag;
	if(parser.ReadBits(1, sps_sub_layer_ordering_info_present_flag) == false)
	{
		return false;
	}

	for(int i=(sps_sub_layer_ordering_info_present_flag?0:max_sub_layers_minus1); i<=max_sub_layers_minus1; i++)
	{
		uint32_t sps_max_dec_pic_buffering_minus1;
		if(parser.ReadUEV(sps_max_dec_pic_buffering_minus1) == false)
		{
			return false;
		}

		uint32_t sps_max_num_reorder_pics;
		if(parser.ReadUEV(sps_max_num_reorder_pics) == false)
		{
			return false;
		}

		uint32_t sps_max_latency_increase_plus1;
		if(parser.ReadUEV(sps_max_latency_increase_plus1) == false)
		{
			return false;
		}
	}

	uint32_t log2_min_luma_coding_block_size_minus3;
	if(parser.ReadUEV(log2_min_luma_coding_block_size_minus3) == false)
	{
		return false;
	}

	uint32_t log2_diff_max_min_luma_coding_block_size;
	if(parser.ReadUEV(log2_diff_max_min_luma_coding_block_size) == false)
	{
		return false;
	}

	uint32_t log2_min_transform_block_size_minus2;
	if(parser.ReadUEV(log2_min_transform_block_size_minus2) == false)
	{
		return false;
	}

	uint32_t log2_diff_max_min_transform_block_size;
	if(parser.ReadUEV(log2_diff_max_min_transform_block_size) == false)
	{
		return false;
	}

	uint32_t max_transform_hierarchy_depth_inter;
	if(parser.ReadUEV(max_transform_hierarchy_depth_inter) == false)
	{
		return false;
	}

	uint32_t max_transform_hierarchy_depth_intra;
	if(parser.ReadUEV(max_transform_hierarchy_depth_intra) == false)
	{
		return false;
	}

	uint8_t scaling_list_enabled_flag;
	if(parser.ReadBits(1, scaling_list_enabled_flag) == false)
	{
		return false;
	}

	if(scaling_list_enabled_flag == 1)
	{
		uint8_t sps_scaling_list_data_present_flag;
		if(parser.ReadBits(1, sps_scaling_list_data_present_flag) == false)
		{
			return false;
		}

		if(sps_scaling_list_data_present_flag == 1)
		{
			// Scaling List Data
			for(int i=0; i<4; i++)
			{
				for(int j=0; j<(i == 3?2:6); j++)
				{
					uint8_t scaling_list_pred_mode_flag;
					if(parser.ReadBits(1, scaling_list_pred_mode_flag) == false)
					{
						return false;
					}

					if(scaling_list_pred_mode_flag == 0)
					{
						uint32_t scaling_list_pred_matrix_id_delta;
						if(parser.ReadUEV(scaling_list_pred_matrix_id_delta) == false)
						{
							return false;
						}
					}
					else
					{
       					uint32_t coef_num = std::min(64, (1 << (4 + (i << 1))));
						if(i > 1)
						{
							int32_t scaling_list_dc_coef_minus8;
							if(parser.ReadSEV(scaling_list_dc_coef_minus8) == false)
							{
								return false;
							}

							for(uint32_t k=0; k<coef_num; k++)
							{
								int32_t scaling_list_delta_coef;
								if(parser.ReadSEV(scaling_list_delta_coef) == false)
								{
									return false;
								}
							}
						}
					}
				}
			}
		}
	}

	uint8_t amp_enabled_flag;
	if(parser.ReadBits(1, amp_enabled_flag) == false)
	{
		return false;
	}

	uint8_t sample_adaptive_offset_enabled_flag;
	if(parser.ReadBits(1, sample_adaptive_offset_enabled_flag) == false)
	{
		return false;
	}

	uint8_t pcm_enabled_flag;
	if(parser.ReadBits(1, pcm_enabled_flag) == false)
	{
		return false;
	}

	if(pcm_enabled_flag == 1)
	{
		uint8_t pcm_sample_bit_depth_luma_minus1;
		if(parser.ReadBits(4, pcm_sample_bit_depth_luma_minus1) == false)
		{
			return false;
		}

		uint8_t pcm_sample_bit_depth_chroma_minus1;
		if(parser.ReadBits(4, pcm_sample_bit_depth_chroma_minus1) == false)
		{
			return false;
		}

		uint32_t log2_min_pcm_luma_coding_block_size_minus3;
		if(parser.ReadUEV(log2_min_pcm_luma_coding_block_size_minus3) == false)
		{
			return false;
		}

		uint32_t log2_diff_max_min_pcm_luma_coding_block_size;
		if(parser.ReadUEV(log2_diff_max_min_pcm_luma_coding_block_size) == false)
		{
			return false;
		}

		uint8_t pcm_loop_filter_disabled_flag;
		if(parser.ReadBits(1, pcm_loop_filter_disabled_flag) == false)
		{
			return false;
		}
	}
	
	// Short term ref pic set

	uint32_t num_short_term_ref_pic_sets;
	if(parser.ReadUEV(num_short_term_ref_pic_sets) == false)
	{
		return false;
	}

	std::vector<ShortTermRefPicSet> rpset_list(num_short_term_ref_pic_sets);
	for(uint32_t i=0; i<num_short_term_ref_pic_sets; i++)
	{
		if(ProcessShortTermRefPicSet(i, num_short_term_ref_pic_sets, rpset_list, parser, rpset_list[i]) == false)
		{
			return false;
		}
	}

	uint8_t long_term_ref_pics_present_flag;
	if(parser.ReadBits(1, long_term_ref_pics_present_flag) == false)
	{
		return false;
	}

	if(long_term_ref_pics_present_flag == 1)
	{
		uint32_t num_long_term_ref_pics_sps;
		if(parser.ReadUEV(num_long_term_ref_pics_sps) == false)
		{
			return false;
		}

		for(uint32_t i = 0; i<num_long_term_ref_pics_sps; i++)
		{
			uint32_t lt_ref_pic_poc_lsb_sps;
			if(parser.ReadBits(log2_max_pic_order_cnt_lsb_minus4 + 4, lt_ref_pic_poc_lsb_sps) == false)
			{
				return false;
			}

			uint8_t used_by_curr_pic_lt_sps_flag;
			if(parser.ReadBits(1, used_by_curr_pic_lt_sps_flag) == false)
			{
				return false;
			}
		}
	}

	uint8_t sps_temporal_mvp_enabled_flag;
	if(parser.ReadBits(1, sps_temporal_mvp_enabled_flag) == false)
	{
		return false;
	}
  	uint8_t strong_intra_smoothing_enabled_flag;
	if(parser.ReadBits(1, strong_intra_smoothing_enabled_flag) == false)
	{
		return false;
	}
  	uint8_t vui_parameters_present_flag;
	if(parser.ReadBits(1, vui_parameters_present_flag) == false)
	{
		return false;
	}

	if(vui_parameters_present_flag == 1)
	{
		VuiParameters params;
		if(ProcessVuiParameters(max_sub_layers_minus1, parser, params) == false)
		{
			return false;
		}

		sps._vui_parameters = params;
	}

	uint8_t sps_extension_flag;
	if(parser.ReadBits(1, sps_extension_flag) == false)
	{
		return false;
	}

	return true;
}

bool H265Parser::ProcessProfileTierLevel(uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, ProfileTierLevel &profile)
{
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
	profile._profile_idc = general_profile_idc;

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
	profile._level_idc = general_level_idc;
	
	std::vector<uint8_t> sub_layer_profile_present_flag_list;
	std::vector<uint8_t> sub_layer_level_present_flag_list;
	for(uint32_t i=0; i<max_sub_layers_minus1; i++)
	{
		uint8_t sub_layer_profile_present_flag;
		if(parser.ReadBits(1, sub_layer_profile_present_flag) == false)
		{
			return false;
		}
		sub_layer_profile_present_flag_list.push_back(sub_layer_profile_present_flag);

		uint8_t sub_layer_level_present_flag;
		if(parser.ReadBits(1, sub_layer_level_present_flag) == false)
		{
			return false;
		}
		sub_layer_level_present_flag_list.push_back(sub_layer_level_present_flag);
	}

	if(max_sub_layers_minus1 > 0)
	{
		for(int i=max_sub_layers_minus1; i<8; i++)
		{
			if(parser.Skip(2) == false)
			{
				return false;
			}
		}
	}

	for(uint32_t i=0; i<max_sub_layers_minus1; i++)
	{
		if(sub_layer_profile_present_flag_list[i])
		{
			// sub_layer_profile_space - 2bits
			// sub_layer_tier_flag - 1 bit
			// sub_layer_profile_idc - 5 bits
			
			// sub_layer_profile_compatibility_flag - 32 bits

			// sub_layer_progressive_source_flag - 1 bit
			// sub_layer_interlaced_source_flag - 1 bit
			// sub_layer_non_packed_constraint_flag - 1 bit
			// sub_layer_frame_only_constraint_flag - 1 bit

			// 32 bits
			// 12 bits
			if(parser.Skip(88) == false)
			{
				return false;
			} 
		}

		if(sub_layer_level_present_flag_list[i])
		{
			// sub_layer_level_idc
			if(parser.Skip(8) == false)
			{
				return false;
			}
		}
	}
	
	return true;
}


bool H265Parser::ProcessVuiParameters(uint32_t sps_max_sub_layers_minus1, NalUnitBitstreamParser &parser, VuiParameters &params)
{
	uint8_t aspect_ratio_info_present_flag;
	if(parser.ReadBits(1, aspect_ratio_info_present_flag) == false)
	{
		return false;
	}

	if(aspect_ratio_info_present_flag == 1)
	{
		uint8_t aspect_ratio_idc;
		if(parser.ReadBits(8, aspect_ratio_idc) == false)
		{
			return false;
		}
		params._aspect_ratio_idc = aspect_ratio_idc;

		// Extended SAR
		if(aspect_ratio_idc == 255)
		{
			uint16_t sar_width, sar_height;

			if(parser.ReadBits(16, sar_width) == false)
			{
				return false;
			}
			if(parser.ReadBits(16, sar_height) == false)
			{
				return false;
			}

			params._aspect_ratio._width = sar_width;
			params._aspect_ratio._height = sar_height;
		}
	}

	uint8_t overscan_info_present_flag;
	if(parser.ReadBits(1, overscan_info_present_flag) == false)
	{
		return false;
	}

	if(overscan_info_present_flag == 1)
	{
		uint8_t overscan_appropriate_flag;
		if(parser.ReadBits(1, overscan_appropriate_flag) == false)
		{
			return false;
		}
	}

	uint8_t video_signal_type_present_flag;
	if(parser.ReadBits(1, video_signal_type_present_flag) == false)
	{
		return false;
	}

	if(video_signal_type_present_flag == 1)
	{
		uint8_t video_format;
		if(parser.ReadBits(3, video_format) == false)
		{
			return false;
		}

		uint8_t video_full_range_flag;
		if(parser.ReadBits(1, video_full_range_flag) == false)
		{
			return false;
		}

		uint8_t colour_description_present_flag;
		if(parser.ReadBits(1, colour_description_present_flag) == false)
		{
			return false;
		}

		if(colour_description_present_flag == 1)
		{
			uint8_t colour_primaries;
			if(parser.ReadBits(8, colour_primaries) == false)
			{
				return false;
			}

			uint8_t transfer_characteristics;
			if(parser.ReadBits(8, transfer_characteristics) == false)
			{
				return false;
			}

			uint8_t matrix_coeffs;
			if(parser.ReadBits(8, matrix_coeffs) == false)
			{
				return false;
			}
		}
	}

	uint8_t chroma_loc_info_present_flag;
	if(parser.ReadBits(1, chroma_loc_info_present_flag) == false)
	{
		return false;
	}

	if(chroma_loc_info_present_flag == 1)
	{
		uint32_t chroma_sample_loc_type_top_field;
		if(parser.ReadUEV(chroma_sample_loc_type_top_field) == false)
		{
			return false;
		}

		uint32_t chroma_sample_loc_type_bottom_field;
		if(parser.ReadUEV(chroma_sample_loc_type_bottom_field) == false)
		{
			return false;
		}
	}

	uint8_t neutral_chroma_indication_flag;
	if(parser.ReadBits(1, neutral_chroma_indication_flag) == false)
	{
		return false;
	}

	uint8_t field_seq_flag;
	if(parser.ReadBits(1, field_seq_flag) == false)
	{
		return false;
	}

	uint8_t frame_field_info_present_flag;
	if(parser.ReadBits(1, frame_field_info_present_flag) == false)
	{
		return false;
	}	

	uint8_t default_display_window_flag;
	if(parser.ReadBits(1, default_display_window_flag) == false)
	{
		return false;
	}	

	if(default_display_window_flag == 1)
	{
		uint32_t def_disp_win_left_offset;
		if(parser.ReadUEV(def_disp_win_left_offset) == false)
		{
			return false;
		}
		uint32_t def_disp_win_right_offset;
		if(parser.ReadUEV(def_disp_win_right_offset) == false)
		{
			return false;
		}
		uint32_t def_disp_win_top_offset;
		if(parser.ReadUEV(def_disp_win_top_offset) == false)
		{
			return false;
		}
		uint32_t def_disp_win_bottom_offset;
		if(parser.ReadUEV(def_disp_win_bottom_offset) == false)
		{
			return false;
		}
	}

	uint8_t vui_timing_info_present_flag;
	if(parser.ReadBits(1, vui_timing_info_present_flag) == false)
	{
		return false;
	}

	if(vui_timing_info_present_flag == 1)
	{
		uint32_t vui_num_units_in_tick;
		if(parser.ReadBits(32, vui_num_units_in_tick) == false)
		{
			return false;
		}
		params._num_units_in_tick = vui_num_units_in_tick;

		uint32_t vui_time_scale;
		if(parser.ReadBits(32, vui_time_scale) == false)
		{
			return false;
		}
		params._time_scale = vui_time_scale;
		
		uint8_t vui_poc_proportional_to_timing_flag;
		if(parser.ReadBits(1, vui_poc_proportional_to_timing_flag) == false)
		{
			return false;
		}

		if(vui_poc_proportional_to_timing_flag == 1)
		{
			uint32_t vui_num_ticks_poc_diff_one_minus1;
			if(parser.ReadUEV(vui_num_ticks_poc_diff_one_minus1) == false)
			{
				return false;
			}
		}

		uint8_t vui_hrd_parameters_present_flag;
		if(parser.ReadBits(1, vui_hrd_parameters_present_flag) == false)
		{
			return false;
		}

		if(vui_hrd_parameters_present_flag == 1)
		{
			HrdParameters params;
			if(ProcessHrdParameters(1, sps_max_sub_layers_minus1, parser, params) == false)
			{
				return false;
			}
		}
	}

	uint8_t bitstream_restriction_flag;
	if(parser.ReadBits(1, bitstream_restriction_flag) == false)
	{
		return false;
	}

	if(bitstream_restriction_flag == 1)
	{
		uint8_t tiles_fixed_structure_flag;
		if(parser.ReadBits(1, tiles_fixed_structure_flag) == false)
		{
			return false;
		}

		uint8_t motion_vectors_over_pic_boundaries_flag;
		if(parser.ReadBits(1, motion_vectors_over_pic_boundaries_flag) == false)
		{
			return false;
		}

		uint8_t restricted_ref_pic_lists_flag;
		if(parser.ReadBits(1, restricted_ref_pic_lists_flag) == false)
		{
			return false;
		}

		uint32_t min_spatial_segmentation_idc;
		if(parser.ReadUEV(min_spatial_segmentation_idc) == false)
		{
			return false;
		}

		uint32_t max_bytes_per_pic_denom;
		if(parser.ReadUEV(max_bytes_per_pic_denom) == false)
		{
			return false;
		}

		uint32_t max_bits_per_min_cu_denom;
		if(parser.ReadUEV(max_bits_per_min_cu_denom) == false)
		{
			return false;
		}

		uint32_t log2_max_mv_length_horizontal;
		if(parser.ReadUEV(log2_max_mv_length_horizontal) == false)
		{
			return false;
		}

		uint32_t log2_max_mv_length_vertical;
		if(parser.ReadUEV(log2_max_mv_length_vertical) == false)
		{
			return false;
		}
	}

	return true;
}

bool H265Parser::ProcessHrdParameters(uint8_t common_inf_present_flag, uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, HrdParameters &params)
{
	uint8_t nal_hrd_parameters_present_flag = 0;
	uint8_t vcl_hrd_parameters_present_flag = 0;
	uint8_t sub_pic_hrd_params_present_flag = 0;
	if(common_inf_present_flag == 1)
	{
		if(parser.ReadBits(1, nal_hrd_parameters_present_flag) == false)
		{
			return false;
		}

		if(parser.ReadBits(1, vcl_hrd_parameters_present_flag) == false)
		{
			return false;
		}

		if(nal_hrd_parameters_present_flag == 1 || vcl_hrd_parameters_present_flag == 1)
		{
			if(parser.ReadBits(1, sub_pic_hrd_params_present_flag) == false)
			{
				return false;
			}

			if(sub_pic_hrd_params_present_flag == 1)
			{
				uint8_t tick_divisor_minus2;
				if(parser.ReadBits(8, tick_divisor_minus2) == false)
				{
					return false;
				}
				uint8_t du_cpb_removal_delay_increment_length_minus1;
				if(parser.ReadBits(5, du_cpb_removal_delay_increment_length_minus1) == false)
				{
					return false;
				}
				uint8_t sub_pic_cpb_params_in_pic_timing_sei_flag;
				if(parser.ReadBits(1, sub_pic_cpb_params_in_pic_timing_sei_flag) == false)
				{
					return false;
				}
				uint8_t dpb_output_delay_du_length_minus1;
				if(parser.ReadBits(5, dpb_output_delay_du_length_minus1) == false)
				{
					return false;
				}
			}

			uint8_t bit_rate_scale;
			if(parser.ReadBits(4, bit_rate_scale) == false)
			{
				return false;
			}
			uint8_t cpb_size_scale;
			if(parser.ReadBits(4, cpb_size_scale) == false)
			{
				return false;
			}

			if(sub_pic_hrd_params_present_flag == 1)
			{
				uint8_t cpb_size_du_scale;
				if(parser.ReadBits(4, cpb_size_du_scale) == false)
				{
					return false;
				}
			}

			uint8_t initial_cpb_removal_delay_length_minus1;
			if(parser.ReadBits(5, initial_cpb_removal_delay_length_minus1) == false)
			{
				return false;
			}
			uint8_t au_cpb_removal_delay_length_minus1;
			if(parser.ReadBits(5, au_cpb_removal_delay_length_minus1) == false)
			{
				return false;
			}
			uint8_t dpb_output_delay_length_minus1;
			if(parser.ReadBits(5, dpb_output_delay_length_minus1) == false)
			{
				return false;
			}
		}
	}

	for(uint32_t i=0; i<=max_sub_layers_minus1; i++)
	{
		uint8_t fixed_pic_rate_general_flag;
		if(parser.ReadBits(1, fixed_pic_rate_general_flag) == false)
		{
			return false;
		}

		uint8_t fixed_pic_rate_within_cvs_flag = 0;
		if(fixed_pic_rate_general_flag == 1)
		{
			fixed_pic_rate_within_cvs_flag = 1;
		}
		else
		{
			if(parser.ReadBits(1, fixed_pic_rate_within_cvs_flag) == false)
			{
				return false;
			}
		}
		
		uint8_t low_delay_hrd_flag = 0;
		if(fixed_pic_rate_within_cvs_flag == 1)
		{
			uint32_t elemental_duration_in_tc_minus1;
			if(parser.ReadUEV(elemental_duration_in_tc_minus1) == false)
			{
				return false;
			}
		}
		else
		{
			if(parser.ReadBits(1, low_delay_hrd_flag) == false)
			{
				return false;
			}
		}
		
		uint32_t cpb_cnt_minus1 = 0;
		if(low_delay_hrd_flag == 1)
		{
			if(parser.ReadUEV(cpb_cnt_minus1) == false)
			{
				return false;
			}
		}

		if(nal_hrd_parameters_present_flag == 1)
		{
			SubLayerHrdParameters params;
			ProcessSubLayerHrdParameters(sub_pic_hrd_params_present_flag, cpb_cnt_minus1, parser, params);
		}

		if(vcl_hrd_parameters_present_flag == 1)
		{
			SubLayerHrdParameters params;
			ProcessSubLayerHrdParameters(sub_pic_hrd_params_present_flag, cpb_cnt_minus1, parser, params);
		}
	}

	return true;
}

bool H265Parser::ProcessSubLayerHrdParameters(uint8_t sub_pic_hrd_params_present_flag, uint32_t cpb_cnt, NalUnitBitstreamParser &parser, SubLayerHrdParameters &params)
{
	for(uint32_t i=0; i<=cpb_cnt; i++)
	{
		uint32_t bit_rate_value_minus1;
		if(parser.ReadUEV(bit_rate_value_minus1) == false)
		{
			return false;
		}

		uint32_t cpb_size_value_minus1;
		if(parser.ReadUEV(cpb_size_value_minus1) == false)
		{
			return false;
		}

		if(sub_pic_hrd_params_present_flag == 1)
		{
			uint32_t cpb_size_du_value_minus1;
			if(parser.ReadUEV(cpb_size_du_value_minus1) == false)
			{
				return false;
			}

			uint32_t bit_rate_du_value_minus1;
			if(parser.ReadUEV(bit_rate_du_value_minus1) == false)
			{
				return false;
			}
		}

		uint8_t cbr_flag;
		if(parser.ReadBits(1, cbr_flag) == false)
		{
			return false;
		}
	}

	return true;
}

bool H265Parser::ProcessShortTermRefPicSet(uint32_t idx, uint32_t num_short_term_ref_pic_sets, const std::vector<ShortTermRefPicSet> &rpset_list, NalUnitBitstreamParser &parser, ShortTermRefPicSet &rpset)
{
	rpset.inter_ref_pic_set_prediction_flag = 0;
	rpset.delta_idx_minus1 = 0;

	if(idx)
	{
		if(parser.ReadBits(1, rpset.inter_ref_pic_set_prediction_flag) == false)
		{
			return false;
		}
	}

	if(rpset.inter_ref_pic_set_prediction_flag == 1)
	{
		if(idx == rpset.inter_ref_pic_set_prediction_flag)
		{
			if(parser.ReadUEV(rpset.delta_idx_minus1) == false)
			{
				return false;
			}
		}

		if(parser.ReadBits(1,rpset.delta_rps_sign) == false)
		{
			return false;
		}

		if(parser.ReadUEV(rpset.abs_delta_rps_minus1) == false)
		{
			return false;
		}

		uint32_t ref_rps_idx = idx - (rpset.delta_idx_minus1 + 1);
    	uint32_t num_delta_pocs = 0;

		if(rpset_list[ref_rps_idx].inter_ref_pic_set_prediction_flag)
		{
			for(uint32_t i=0; i<rpset_list[ref_rps_idx].used_by_curr_pic_flag.size(); i++)
			{
				if(rpset_list[ref_rps_idx].used_by_curr_pic_flag[i] || rpset_list[ref_rps_idx].use_delta_flag[i])
				{
					num_delta_pocs ++;
				}
			}
		}
		else
		{
			num_delta_pocs = rpset_list[ref_rps_idx].num_negative_pics + rpset_list[ref_rps_idx].num_positive_pics;
		}
		
		rpset.used_by_curr_pic_flag.resize(num_delta_pocs + 1);
    	rpset.use_delta_flag.resize(num_delta_pocs + 1, 1);

		for(uint32_t i=0; i<num_delta_pocs; i++)
		{
			if(parser.ReadBits(1, rpset.used_by_curr_pic_flag[i]) == false)
			{
				return false;
			}
			if(!rpset.used_by_curr_pic_flag[i])
			{
				if(parser.ReadBits(1, rpset.use_delta_flag[i]) == false)
				{
					return false;
				}
			}
		}
	}
	else
	{
		if(parser.ReadUEV(rpset.num_negative_pics) == false || parser.ReadUEV(rpset.num_positive_pics) == false)
		{
			return false;
		}

		rpset.delta_poc_s0_minus1.resize(rpset.num_negative_pics);
   		rpset.used_by_curr_pic_s0_flag.resize(rpset.num_negative_pics);
		
		for(std::size_t i=0; i<rpset.num_negative_pics; i++)
		{
			if(parser.ReadUEV(rpset.delta_poc_s0_minus1[i]) == false)
			{
				return false;
			}
			if(parser.ReadBits(1, rpset.used_by_curr_pic_s0_flag[i]) == false)
			{
				return false;
			}
		}

		rpset.delta_poc_s1_minus1.resize(rpset.num_positive_pics);
		rpset.used_by_curr_pic_s1_flag.resize(rpset.num_positive_pics);
		for(std::size_t i=0; i<rpset.num_positive_pics; i++)
		{
			if(parser.ReadUEV(rpset.delta_poc_s1_minus1[i]) == false)
			{
				return false;
			}
			if(parser.ReadBits(1, rpset.used_by_curr_pic_s1_flag[i]) == false)
			{
				return false;
			}
		}
	}
	return true;
}
