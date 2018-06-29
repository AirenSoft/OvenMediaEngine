//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>
#include <vector>
#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavutil/opt.h>
}

#include "base/application/application.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "../transcode_context.h"

class MediaFilterImpl
{
public:
	MediaFilterImpl() {};
    virtual ~MediaFilterImpl() {};

    // 원본 스트림 정보
    // stream_info : 원본 파일 정보
    // context : 변환 정보
    virtual int32_t Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context) = 0;

    virtual int32_t SendBuffer(std::unique_ptr<MediaBuffer> buf) = 0;

    virtual std::pair<int32_t, std::unique_ptr<MediaBuffer>> RecvBuffer() = 0;
};

