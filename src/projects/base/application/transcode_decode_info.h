//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_video_info.h"

class TranscodeDecodeInfo
{
public:
    TranscodeDecodeInfo();
    virtual ~TranscodeDecodeInfo();

    std::shared_ptr<const TranscodeVideoInfo> GetVideo() const noexcept;
    void SetVideo(std::shared_ptr<TranscodeVideoInfo> video);

    ov::String ToString() const;

private:
    std::shared_ptr<TranscodeVideoInfo> _video;
};
