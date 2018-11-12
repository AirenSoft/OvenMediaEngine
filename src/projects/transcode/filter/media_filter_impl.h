//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../transcode_context.h"
#include "../codec/transcode_base.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <algorithm>
#include <thread>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>
#include <base/application/application.h>

class MediaFilterImpl
{
public:
	MediaFilterImpl() = default;
	virtual ~MediaFilterImpl() = default;

	// 원본 스트림 정보
	// stream_info : 원본 파일 정보
	// context : 변환 정보
	virtual bool Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context) = 0;

	virtual int32_t SendBuffer(std::unique_ptr<MediaFrame> buffer) = 0;
	virtual std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) = 0;

};

