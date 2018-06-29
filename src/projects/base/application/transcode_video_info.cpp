//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_video_info.h"

TranscodeVideoInfo::TranscodeVideoInfo()
{
}

TranscodeVideoInfo::~TranscodeVideoInfo()
{
}

const bool TranscodeVideoInfo::GetActive() const noexcept
{
    return _active;
}

void TranscodeVideoInfo::SetActive(bool active)
{
    _active = active;
}

const ov::String TranscodeVideoInfo::GetCodec() const noexcept
{
    return _codec;
}

void TranscodeVideoInfo::SetCodec(ov::String codec)
{
    _codec = codec;
}

const ov::String TranscodeVideoInfo::GetHWAcceleration() const noexcept
{
    return _hw_acceleration;
}

void TranscodeVideoInfo::SetHWAcceleration(ov::String hw_acceleration)
{
    _hw_acceleration = hw_acceleration;
}

const ov::String TranscodeVideoInfo::GetScale() const noexcept
{
    return _scale;
}

void TranscodeVideoInfo::SetScale(ov::String scale)
{
    _scale = scale;
}

const int TranscodeVideoInfo::GetWidth() const noexcept
{
    return _width;
}

void TranscodeVideoInfo::SetWidth(int width)
{
    _width = width;
}

const int TranscodeVideoInfo::GetHeight() const noexcept
{
    return _height;
}

void TranscodeVideoInfo::SetHeight(int height)
{
    _height = height;
}

const int TranscodeVideoInfo::GetBitrate() const noexcept
{
    return _bitrate;
}

void TranscodeVideoInfo::SetBitrate(int bitrate)
{
    _bitrate = bitrate;
}

const float TranscodeVideoInfo::GetFramerate() const noexcept
{
    return _framerate;
}

void TranscodeVideoInfo::SetFramerate(float framerate)
{
    _framerate = framerate;
}

const ov::String TranscodeVideoInfo::GetH264Profile() const noexcept
{
    return _h264_profile;
}

void TranscodeVideoInfo::SetH264Profile(ov::String h264_profile)
{
    _h264_profile = h264_profile;
}

const ov::String TranscodeVideoInfo::GetH264Level() const noexcept
{
    return _h264_level;
}

void TranscodeVideoInfo::SetH264Level(ov::String h264_level)
{
    _h264_level = h264_level;
}

ov::String TranscodeVideoInfo::ToString() const
{
    return ov::String::FormatString("{\"active\": %d, \"codec\": \"%s\", \"hw_acceleration\": \"%s\", \"scale\": \"%s\", \"width\": %d, \"height\": %d, \"bitrate\": %d, \"framerate\": %f, \"h264_profile\": \"%s\", \"h264_level\": \"%s\"}",
                                    _active, _codec.CStr(), _hw_acceleration.CStr(), _scale.CStr(), _width, _height, _bitrate, _framerate, _h264_profile.CStr(), _h264_level.CStr());
}