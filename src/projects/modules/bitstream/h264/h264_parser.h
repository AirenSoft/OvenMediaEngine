#pragma once

#include <modules/bitstream/nalu/nal_unit_bitstream_parser.h>
#include <base/ovlibrary/ovlibrary.h>
#include <cstdint>

#include "h264_nal_unit_types.h"

#define H264_NAL_UNIT_HEADER_SIZE 1

class H264SPS
{
    struct ASPECT_RATIO
    {
        uint16_t _width;
        uint16_t _height;
    };

public:
    
    unsigned int GetWidth() const
    {
        return _width;
    }
    unsigned int GetHeight() const
    {
        return _height;
    }
    uint8_t GetProfileIdc() const
    {
        return _profile;
    }
	uint8_t GetConstraintFlag() const
	{
		return _constraint;
	}
    uint8_t GetCodecLevelIdc() const
    {
        return _codec_level;
    }
    unsigned int GetFps() const
    {
        return _fps;
    }
    unsigned int GetId() const
    {
        return _id;
    }
    unsigned int GetMaxNrOfReferenceFrames() const
    {
        return _max_nr_of_reference_frames;
    }

    ov::String GetInfoString()
    {
        ov::String out_str = ov::String::FormatString("\n[H264Sps]\n");

        out_str.AppendFormat("\tProfile(%d)\n", GetProfileIdc());
        out_str.AppendFormat("\tCodecLevel(%d)\n", GetCodecLevelIdc());
        out_str.AppendFormat("\tWidth(%d)\n", GetWidth());
        out_str.AppendFormat("\tHeight(%d)\n", GetHeight());
        out_str.AppendFormat("\tFps(%d)\n", GetFps());
        out_str.AppendFormat("\tId(%d)\n", GetId());
        out_str.AppendFormat("\tMaxNrOfReferenceFrames(%d)\n", GetMaxNrOfReferenceFrames());
        out_str.AppendFormat("\tAspectRatio(IDC - %d Extented - %d:%d)\n", _aspect_ratio_idc, _aspect_ratio._width, _aspect_ratio._height);

        return out_str;
    }

private:
    uint8_t _profile = 0;
	uint8_t _constraint = 0;
    uint8_t _codec_level = 0;
    unsigned int _width = 0;
    unsigned int _height = 0;
    unsigned int _fps = 0;
    unsigned int _id = 0;
    unsigned int _max_nr_of_reference_frames = 0;
    uint8_t _aspect_ratio_idc = 0;
    ASPECT_RATIO _aspect_ratio = { 0, 0 };

    friend class H264Parser;
};

class H264NalUnitHeader
{
public:
    H264NalUnitType GetNalUnitType()
    {
        return _type;
    }
private:
    uint8_t _nal_ref_idc = 0;
    H264NalUnitType _type = H264NalUnitType::Unspecified;

    friend class H264Parser;
};


class H264Parser
{
public:
    static bool CheckKeyframe(const uint8_t *bitstream, size_t length);
    static bool ParseNalUnitHeader(const uint8_t *nalu, size_t length, H264NalUnitHeader &header);
    static bool ParseSPS(const uint8_t *nalu, size_t length, H264SPS &sps);

private:
    static bool ParseNalUnitHeader(NalUnitBitstreamParser &parser, H264NalUnitHeader &header);
};
