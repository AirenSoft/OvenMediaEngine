//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class TranscodeVideoInfo
{
public:
    TranscodeVideoInfo();
    virtual ~TranscodeVideoInfo();

    const bool GetActive() const noexcept;
    void SetActive(bool active);

    const ov::String GetCodec() const noexcept;
    void SetCodec(ov::String codec);

    const ov::String GetHWAcceleration() const noexcept;
    void SetHWAcceleration(ov::String hw_acceleration);

    const ov::String GetScale() const noexcept;
    void SetScale(ov::String scale);

    const int GetWidth() const noexcept;
    void SetWidth(int width);

    const int GetHeight() const noexcept;
    void SetHeight(int height);

    const int GetBitrate() const noexcept;
    void SetBitrate(int bitrate);

    const float GetFramerate() const noexcept;
    void SetFramerate(float framerate);

    const ov::String GetH264Profile() const noexcept;
    void SetH264Profile(ov::String h264_profile);

    const ov::String GetH264Level() const noexcept;
    void SetH264Level(ov::String h264_level);

    ov::String ToString() const;

private:
    bool _active;
    ov::String _codec;
    ov::String _hw_acceleration;
    ov::String _scale;
    int _width;
    int _height;
    int _bitrate;
    float _framerate;

    ov::String _h264_profile;
    ov::String _h264_level;
};
