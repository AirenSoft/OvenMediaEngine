//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "filter_fps.h"

#include <base/ovlibrary/ovlibrary.h>

#include "../transcoder_private.h"

FilterFps::FilterFps()
{
	_input_framerate = 0;
	_output_framerate = 0;
	_skip_frames = 0;

	_next_pts = AV_NOPTS_VALUE;

	stat_output_frame_count = 0;
}

FilterFps::~FilterFps()
{
	_frames.clear();
}

void FilterFps::SetInputTimebase(cmn::Timebase timebase)
{
	_input_timebase = timebase;
}

void FilterFps::SetInputFrameRate(double framerate)
{
	_input_framerate = framerate;
}

void FilterFps::SetOutputFrameRate(double framerate)
{
	_output_framerate = framerate;
}

void FilterFps::SetSkipFrames(int32_t skip_frames)
{
	_skip_frames = skip_frames;
}

bool FilterFps::Push(std::shared_ptr<MediaFrame> media_frame)
{
	stat_input_frame_count++;
	
	if (_frames.size() >= 2)
	{
		logtw("FPS filter is full");

		return false;
	}

	int64_t pts = media_frame->GetPts();
	int64_t scaled_pts = av_rescale_q_rnd(pts,
										   (AVRational){_input_timebase.GetNum(), _input_timebase.GetDen()},
										  av_inv_q(av_d2q(_output_framerate, INT_MAX)),
										  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	media_frame->SetPts(scaled_pts);

	if (_next_pts == AV_NOPTS_VALUE)
	{
		logti("set first pts(%lld), rescale_pts(%lld)", pts, media_frame->GetPts());
		_next_pts = media_frame->GetPts();
	}

	_frames.push_back(media_frame);

	return true;
}

std::shared_ptr<MediaFrame> FilterFps::Pop()
{
	while (_frames.size() >= 2)
	{
		if (_frames.size() == 2 && _frames[1]->GetPts() <= _next_pts)
		{
			_frames.erase(_frames.begin());

			break;
		}

		int64_t resotre_pts = av_rescale_q_rnd(_next_pts,
											   av_inv_q(av_d2q(_output_framerate, INT_MAX)),
											   (AVRational){_input_timebase.GetNum(), _input_timebase.GetDen()},
											   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

		_next_pts++;

		// Skip Frame
		if ((_skip_frames > 0) && (stat_output_frame_count++ % (_skip_frames + 1) != 0))
		{
			continue;
		}

		auto clone_frame = _frames[0]->CloneFrame();
		clone_frame->SetPts(resotre_pts);

		return clone_frame;
	}

	return nullptr;
}
