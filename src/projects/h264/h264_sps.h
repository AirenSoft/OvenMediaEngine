#pragma once

#include "h264_nal_unit_bitstream_parser.h"

#include <cstdint>

class H264Sps
{
    struct ASPECT_RATIO
    {
        uint16_t width_;
        uint16_t height_;
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
    uint8_t profile_ = 0;
    uint8_t codec_level_ = 0;
    unsigned int width_ = 0;
    unsigned int height_ = 0;
    unsigned int fps_ = 0;
    unsigned int id_ = 0;
    unsigned int max_nr_of_reference_frames_ = 0;
    ASPECT_RATIO aspect_ratio_ = { 0, 0 };
};
