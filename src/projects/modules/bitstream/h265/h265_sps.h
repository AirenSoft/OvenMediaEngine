// I developed the code of this file by referring to the source code of virinext's hevcsbrowser (https://github.com/virinext/hevcesbrowser). 
// Thanks to virinext.
// - Getroot

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/bitstream/nalu/nal_unit_bitstream_parser.h>
#include <cstdint>

struct ShortTermRefPicSet
{
public:
    uint8_t                   inter_ref_pic_set_prediction_flag;
    uint32_t                  delta_idx_minus1;
    uint8_t                   delta_rps_sign;
    uint32_t                  abs_delta_rps_minus1;
    std::vector<uint8_t>      used_by_curr_pic_flag;
    std::vector<uint8_t>      use_delta_flag;
    uint32_t                  num_negative_pics;
    uint32_t                  num_positive_pics;
    std::vector<uint32_t>     delta_poc_s0_minus1;
    std::vector<uint8_t>      used_by_curr_pic_s0_flag;
    std::vector<uint32_t>     delta_poc_s1_minus1;
    std::vector<uint8_t>      used_by_curr_pic_s1_flag;
};

class H265Sps
{
    struct ASPECT_RATIO
    {
        uint16_t _width;
        uint16_t _height;
    };

public:
    static bool Parse(const uint8_t *sps_bitstream, size_t length, H265Sps &sps);

    unsigned int GetWidth() const;
    unsigned int GetHeight() const;
    uint8_t GetProfile() const;
    uint8_t GetCodecLevel() const;
    unsigned int GetFps() const;
    unsigned int GetId() const;
    unsigned int GetMaxNrOfReferenceFrames() const;

    ov::String GetInfoString();

private:
    static bool ProcessShortTermRefPicSet(uint32_t idx, uint32_t num_short_term_ref_pic_sets, ShortTermRefPicSet &rpset, const std::vector<ShortTermRefPicSet> &rpset_list, NalUnitBitstreamParser &parser, H265Sps &sps);
    static bool ProcessVuiParameters(uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, H265Sps &sps);
    static bool ProcessHrdParameters(uint8_t common_inf_present_flag, uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, H265Sps &sps);
    static bool ProcessSubLayerHrdParameters(uint8_t sub_pic_hrd_params_present_flag, uint32_t cpb_cnt, NalUnitBitstreamParser &parser, H265Sps &sps);

    uint8_t _profile = 0;
    uint8_t _codec_level = 0;
    unsigned int _width = 0;
    unsigned int _height = 0;
    unsigned int _fps = 0;
    unsigned int _id = 0;
    unsigned int _max_nr_of_reference_frames = 0;
    ASPECT_RATIO _aspect_ratio = { 0, 0 };
};
