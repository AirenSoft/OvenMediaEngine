//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "filter_video_base.h"
#include "scale_geometry.h"

#include <modules/ffmpeg/compat.h>
#include <modules/ffmpeg/ffmpeg_filter_graph.h>
#include <modules/ffmpeg/ffmpeg_frame_canvas.h>

class FilterLavfiRescaler : public FilterVideoBase
{
public:
	~FilterLavfiRescaler() override;

	// ----- Filter interface -----
	bool Initialize() override;
	void Uninitialize() override;
	bool SendFrame(std::shared_ptr<MediaFrame> media_frame) override;
	std::shared_ptr<MediaFrame> ReceiveFrame() override;

private:
	// ----- Internal helpers (FFmpeg avfilter graph) -----
	bool BuildDescription(ov::String &desc);
	bool InitializeSourceFilter();
	bool InitializeSinkFilter();
	bool InitializeFilterDescription();

	// Places the scaled frame on a black output-sized frame (fit).
	// The pad filter is not part of the FFmpeg build, so this is done here.
	std::shared_ptr<MediaFrame> CopyToCanvas(const std::shared_ptr<MediaFrame> &scaled_frame);

	// ----- Members -----
	ffmpeg::FFmpegFilterGraph _graph;

	ScaleGeometry _geometry;
	ffmpeg::FrameCanvas _canvas;
#if _SIMULATE_PROCESSING_DELAY_ENABLED
	int32_t _simulate_overload = 0;
#endif
};
