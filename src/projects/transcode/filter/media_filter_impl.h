//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "../codec/transcode_base.h"
#include "../transcode_context.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <base/info/application.h>
#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>

class MediaFilterImpl
{
public:
	MediaFilterImpl() = default;
	virtual ~MediaFilterImpl() = default;

	// 원본 스트림 정보
	// stream_info : 원본 파일 정보
	// context : 변환 정보
	virtual bool Configure(const std::shared_ptr<MediaTrack> &input_media_track, const std::shared_ptr<TranscodeContext> &input_context, const std::shared_ptr<TranscodeContext> &output_context) = 0;

	virtual int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer) = 0;
	virtual std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) = 0;

	static AVRational TimebaseToAVRational(const cmn::Timebase &timebase)
	{
		return (AVRational){
			.num = timebase.GetNum(),
			.den = timebase.GetDen()
		};
	}

	uint32_t GetInputBufferSize()
	{
		return _input_buffer.size();
	}

	uint32_t GetOutputBufferSize()
	{
		return _output_buffer.size();
	}

	cmn::Timebase GetInputTimebase() const
	{
		return _input_context->GetTimeBase();
	}

	cmn::Timebase GetOutputTimebase() const
	{
		return _output_context->GetTimeBase();
	}



protected:
	std::deque<std::shared_ptr<MediaFrame>> _input_buffer;
	std::deque<std::shared_ptr<MediaFrame>> _output_buffer;

	AVFrame *_frame = nullptr;
	AVFilterContext *_buffersink_ctx = nullptr;
	AVFilterContext *_buffersrc_ctx = nullptr;
	AVFilterGraph *_filter_graph = nullptr;
	AVFilterInOut *_inputs = nullptr;
	AVFilterInOut *_outputs = nullptr;

	double _scale = 0.0;

	std::shared_ptr<TranscodeContext> _input_context;
	std::shared_ptr<TranscodeContext> _output_context;

	bool _kill_flag = false;
	std::mutex _mutex;
	std::thread _thread_work;
	ov::Semaphore _queue_event;
};

