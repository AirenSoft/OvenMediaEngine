//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_decoder.h"

class OvenCodecImplAvcodecDecAVC : public TranscodeDecoder
{
public:
    AVCodecID GetCodecID() const noexcept override
    {
        return AV_CODEC_ID_H264;
    }

    std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;
};
