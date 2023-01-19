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

#include "../codec/codec_base.h"
#include "../transcoder_context.h"

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

class FilterBase
{
public:
	typedef std::function<void(std::shared_ptr<MediaFrame>)> CompleteHandler;
	FilterBase() = default;
	virtual ~FilterBase() = default;

	virtual bool Configure(const std::shared_ptr<MediaTrack> &input_track, const std::shared_ptr<MediaTrack> &output_track) = 0;

	virtual int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer) = 0;
	virtual bool Start() = 0;
	virtual void Stop() = 0;
	
	cmn::Timebase GetInputTimebase() const
	{
		return _input_track->GetTimeBase();
	}

	cmn::Timebase GetOutputTimebase() const
	{
		return _output_track->GetTimeBase();
	}

	int32_t GetInputWidth() const 
	{
		return _input_width;
	}

	int32_t GetInputHeight() const 
	{
		return _input_height;
	}

	void SetCompleteHandler(CompleteHandler complete_handler) {
		_complete_handler = complete_handler;
	}

protected:
	ov::Queue<std::shared_ptr<MediaFrame>> _input_buffer;

	AVFrame *_frame = nullptr;
	AVFilterContext *_buffersink_ctx = nullptr;
	AVFilterContext *_buffersrc_ctx = nullptr;
	AVFilterGraph *_filter_graph = nullptr;
	AVFilterInOut *_inputs = nullptr;
	AVFilterInOut *_outputs = nullptr;

	double _scale = 0.0;

	// resolution of the input video frame
	int32_t _input_width = 0;
	int32_t _input_height = 0;

	std::shared_ptr<MediaTrack> _input_track;
	std::shared_ptr<MediaTrack> _output_track;

	bool _kill_flag = false;
	std::thread _thread_work;

	CompleteHandler _complete_handler;
};
