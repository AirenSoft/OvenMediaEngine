//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <vector>
#include <stdint.h>

#include "media_filter_impl.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"

#include "../transcode_context.h"

class MediaFilterResampler : public MediaFilterImpl
{
public:
	MediaFilterResampler();
	~MediaFilterResampler();

	bool Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context) override;

	int32_t SendBuffer(std::unique_ptr<MediaFrame> buffer) override;
	std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;

protected:
	bool IsPlanar(AVSampleFormat format);

	AVFrame *_frame;
	AVFilterContext *_buffersink_ctx;
	AVFilterContext *_buffersrc_ctx;
	AVFilterGraph *_filter_graph;
	AVFilterInOut *_outputs;
	AVFilterInOut *_inputs;

	std::vector<std::unique_ptr<MediaFrame>> _input_buffer;
};
