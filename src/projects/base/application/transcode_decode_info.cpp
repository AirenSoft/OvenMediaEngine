//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_decode_info.h"

TranscodeDecodeInfo::TranscodeDecodeInfo()
{
}

TranscodeDecodeInfo::~TranscodeDecodeInfo()
{
}

std::shared_ptr<const TranscodeVideoInfo> TranscodeDecodeInfo::GetVideo() const noexcept
{
    return _video;
}

void TranscodeDecodeInfo::SetVideo(std::shared_ptr<TranscodeVideoInfo> video)
{
    _video = video;
}

ov::String TranscodeDecodeInfo::ToString() const
{
    ov::String result = ov::String::FormatString("{");

    if(_video != nullptr)
    {
        result.AppendFormat("\"video\": [%s]", _video->ToString().CStr());
    }

    result.Append("}");

    return result;
}