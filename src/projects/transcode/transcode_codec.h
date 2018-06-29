#pragma once

#include <stdint.h>


#include "codec/transcode_codec_impl.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "transcode_context.h"


class TranscodeCodec
{
public:
    TranscodeCodec();
    TranscodeCodec(MediaCodecId codec_id, bool is_encoder, std::shared_ptr<TranscodeContext> context = nullptr);
    ~TranscodeCodec();

    void Configure(MediaCodecId codec_id, bool is_encoder = false, std::shared_ptr<TranscodeContext> context = nullptr); 

    int32_t SendBuffer(std::unique_ptr<MediaBuffer> buf);

    std::pair<int32_t, std::unique_ptr<MediaBuffer>> RecvBuffer();
    
private:
	OvenCodecImpl* 	_impl;
};

