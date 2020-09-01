// I developed the code of this file by referring to the source code of virinext's hevcsbrowser (https://github.com/virinext/hevcesbrowser). 
// Thanks to virinext.
// - Getroot

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/bitstream/nalu/nal_unit_bitstream_parser.h>
#include <cstdint>

#include "h265_types.h"

struct ProfileTierLevel
{

};

struct HrdParameters
{

};

struct SubLayerHrdParameters
{

};

struct VuiParameters
{
public:
    struct ASPECT_RATIO
    {
        uint16_t _width;
        uint16_t _height;
    };

    ASPECT_RATIO _aspect_ratio = { 0, 0 };
};

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

class H265NalUnitHeader
{
public:
    H265NALUnitType GetNalUnitType()
    {
        return _type;
    }

    uint8_t GetLayerId()
    {
        return _layer_id;
    }

    uint8_t GetTemporalIdPlus1()
    {
        return _temporal_id_plus1;
    }
private:
    H265NALUnitType _type = H265NALUnitType::UNKNOWN;
    uint8_t _layer_id = 0;
    uint8_t _temporal_id_plus1 = 0;

    friend class H265Parser;
};

class H265SPS
{
public:
    unsigned int GetWidth() const;
    unsigned int GetHeight() const;
    uint8_t GetProfile() const;
    uint8_t GetCodecLevel() const;
    unsigned int GetFps() const;
    unsigned int GetId() const;
    unsigned int GetMaxNrOfReferenceFrames() const;

    ov::String GetInfoString();
private:
    uint8_t _profile = 0;
    uint8_t _codec_level = 0;
    unsigned int _width = 0;
    unsigned int _height = 0;
    unsigned int _fps = 0;
    unsigned int _id = 0;
    unsigned int _max_nr_of_reference_frames = 0;

    VuiParameters   _vui_parameters;

    friend class H265Parser;
};

class H265Parser
{
public:
    static bool ParseNalUnitHeader(const uint8_t *nalu, size_t length, H265NalUnitHeader &header);
    static bool ParseSPS(const uint8_t *nalu, size_t length, H265SPS &sps);

private:
    static bool ParseNalUnitHeader(NalUnitBitstreamParser &parser, H265NalUnitHeader &header);
    static bool ProcessProfileTierLevel(uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, ProfileTierLevel &profile);
    static bool ProcessShortTermRefPicSet(uint32_t idx, uint32_t num_short_term_ref_pic_sets, const std::vector<ShortTermRefPicSet> &rpset_list, NalUnitBitstreamParser &parser, ShortTermRefPicSet &rpset);
    static bool ProcessVuiParameters(uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, VuiParameters &params);
    static bool ProcessHrdParameters(uint8_t common_inf_present_flag, uint32_t max_sub_layers_minus1, NalUnitBitstreamParser &parser, HrdParameters &params);
    static bool ProcessSubLayerHrdParameters(uint8_t sub_pic_hrd_params_present_flag, uint32_t cpb_cnt, NalUnitBitstreamParser &parser, SubLayerHrdParameters &params);
};
