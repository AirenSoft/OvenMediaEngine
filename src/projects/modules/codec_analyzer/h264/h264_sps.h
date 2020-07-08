#pragma once

#include "h264_nal_unit_bitstream_parser.h"

#include <cstdint>

class H264Sps
{
    struct ASPECT_RATIO
    {
        uint16_t _width;
        uint16_t _height;
    };

public:
    static bool Parse(const uint8_t *sps_bitstream, size_t length, H264Sps &sps);

    unsigned int GetWidth() const;
    unsigned int GetHeight() const;
    uint8_t GetProfile() const;
    uint8_t GetCodecLevel() const;
    unsigned int GetFps() const;
    unsigned int GetId() const;
    unsigned int GetMaxNrOfReferenceFrames() const;

private:
    uint8_t _profile = 0;
    uint8_t _codec_level = 0;
    unsigned int _width = 0;
    unsigned int _height = 0;
    unsigned int _fps = 0;
    unsigned int _id = 0;
    unsigned int _max_nr_of_reference_frames = 0;
    ASPECT_RATIO _aspect_ratio = { 0, 0 };
};
