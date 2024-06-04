//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../transcoder_context.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"
#include "filter_base.h"

class FilterFps 
{
public:
	FilterFps();
	~FilterFps();

	void SetInputTimebase(cmn::Timebase timebase);
	void SetInputFrameRate(double framerate);
	void SetOutputFrameRate(double framerate);
	double GetOutputFrameRate();
	void SetSkipFrames(int32_t skip_frames);
	void SetMaximumDupulicateFrames(int32_t max_dupulicate_frames);
	bool Push(std::shared_ptr<MediaFrame> media_frame);
	std::shared_ptr<MediaFrame> Pop();

	ov::String GetStatsString();
	ov::String GetInfoString();

private:
	cmn::Timebase _input_timebase;
	double _input_framerate;
	double _output_framerate;

	// Maximum number of duplicate frames. 
	// If the number of duplicate frames exceeds this value, the frame is discarded.
	// Prevents the frame from being duplicated indefinitely.
	int32_t _max_dupulicate_frames = 5;

	// The number of frames to skip based on the output framerate
	// If 0, do not skip
	int32_t _skip_frames = 0;
	
	// Buffer for storing frames
	std::vector<std::shared_ptr<MediaFrame>> _frames;

	// The next PTS to be output
	int64_t _curr_pts;
	int64_t _next_pts;
	
	int64_t stat_input_frame_count = 0;
	int64_t stat_output_frame_count = 0;
	int64_t stat_skip_frame_count = 0;
	int64_t stat_duplicate_frame_count = 0;
	int64_t stat_discard_frame_count = 0;
};
