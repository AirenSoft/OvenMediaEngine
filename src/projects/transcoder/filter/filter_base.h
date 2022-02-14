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

class MediaFilterImpl
{
public:
	MediaFilterImpl() = default;
	virtual ~MediaFilterImpl() = default;

	virtual bool Configure(const std::shared_ptr<MediaTrack> &input_media_track, const std::shared_ptr<TranscodeContext> &input_context, const std::shared_ptr<TranscodeContext> &output_context) = 0;

	virtual int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer) = 0;
	virtual std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) = 0;
	virtual bool Start() = 0;
	
	static AVRational TimebaseToAVRational(const cmn::Timebase &timebase)
	{
		return (AVRational){
			.num = timebase.GetNum(),
			.den = timebase.GetDen()};
	}

	uint32_t GetInputBufferSize()
	{
		return _input_buffer.Size();
	}

	uint32_t GetOutputBufferSize()
	{
		return _output_buffer.Size();
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
	ov::Queue<std::shared_ptr<MediaFrame>> _input_buffer;
	ov::Queue<std::shared_ptr<MediaFrame>> _output_buffer;

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
	std::thread _thread_work;
};
