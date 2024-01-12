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
#include <stdint.h>
#include <memory>
#include <thread>
#include <vector>

#include "../codec/codec_base.h"
#include "../transcoder_context.h"

#include <base/info/application.h>
#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>
#include <modules/ffmpeg/ffmpeg_conv.h>
#include <modules/managed_queue/managed_queue.h>

class FilterBase
{
public:
	enum class State : uint8_t {
		CREATED,
		STARTED,
		STOPPED,
		ERROR
	};

	typedef std::function<void(std::shared_ptr<MediaFrame>)> CompleteHandler;
	FilterBase() = default;
	virtual ~FilterBase() = default;

	virtual bool Configure(const std::shared_ptr<MediaTrack> &input_track, const std::shared_ptr<MediaTrack> &output_track) = 0;
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
		return _src_width;
	}

	int32_t GetInputHeight() const 
	{
		return _src_height;
	}

	void SetCompleteHandler(CompleteHandler complete_handler) {
		_complete_handler = complete_handler;
	}

	void SetQueueUrn(std::shared_ptr<info::ManagedQueue::URN> &urn) {
		_input_buffer.SetUrn(urn);
	}

	void SetState(State state)
	{
		_state = state;
	}

	State GetState() const
	{
		return _state;
	}

	bool SendBuffer(std::shared_ptr<MediaFrame> buffer)
	{
		if(GetState() == State::CREATED || GetState() == State::STARTED)
		{
			_input_buffer.Enqueue(std::move(buffer));

			return true;
		}

		return false;
	}

	void Complete(std::shared_ptr<MediaFrame> buffer)
	{
		if (_complete_handler != nullptr && _kill_flag == false)
		{
			_complete_handler(std::move(buffer));
		}
	}

protected:

	std::atomic<State> _state = State::CREATED;

	ov::ManagedQueue<std::shared_ptr<MediaFrame>> _input_buffer;

	AVFrame *_frame = nullptr;

	int32_t 	_src_pixfmt = 0;
	int32_t 	_src_width = 0;
	int32_t 	_src_height = 0;

	ov::String 	_src_args = "";
	
	ov::String 	_filter_desc = "";

	AVFilterContext *_buffersink_ctx = nullptr;
	AVFilterContext *_buffersrc_ctx = nullptr;
	AVFilterGraph *_filter_graph = nullptr;
	AVFilterInOut *_inputs = nullptr;
	AVFilterInOut *_outputs = nullptr;

	const AVFilter *_buffersrc = nullptr;
	const AVFilter *_buffersink = nullptr;
	
	// double _scale = 0.0;

	// resolution of the input video frame
	std::shared_ptr<MediaTrack> _input_track;
	std::shared_ptr<MediaTrack> _output_track;

	bool _kill_flag = false;
	std::thread _thread_work;

	CompleteHandler _complete_handler;

	bool _use_hwframe_transfer = false;
};
