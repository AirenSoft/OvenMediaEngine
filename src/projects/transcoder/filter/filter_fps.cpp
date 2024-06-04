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
	if (_next_pts != AV_NOPTS_VALUE)
	{
		int64_t scaled_next_pts = av_rescale_q_rnd(_next_pts,
									 av_inv_q(av_d2q(_output_framerate, INT_MAX)),
									 av_inv_q(av_d2q(framerate, INT_MAX)),
									 (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

		// logtd("Change NextPTS : %lld -> %lld", _next_pts, scaled_next_pts);

		_next_pts = scaled_next_pts;										 
	}

	_output_framerate = framerate;

}

double FilterFps::GetOutputFrameRate()
{
	return _output_framerate;
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

	// Changed from Timebase PTS to Framerate PTS.
	//  ex) 1/90000 -> 1/30
	int64_t framerate_pts = av_rescale_q_rnd(media_frame->GetPts(),
											 (AVRational){_input_timebase.GetNum(), _input_timebase.GetDen()},
											 av_inv_q(av_d2q(_output_framerate, INT_MAX)),
											 (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	// logtd("Push Frame. PTS(%lld) -> PTS(%lld)", media_frame->GetPts(), framerate_pts);
	media_frame->SetPts(framerate_pts);

	if (_next_pts == AV_NOPTS_VALUE)
	{
		_next_pts = media_frame->GetPts();
	}

	_frames.push_back(media_frame);

	return true;
}

std::shared_ptr<MediaFrame> FilterFps::Pop()
{
	while (_frames.size() >= 2)
	{
		if (_frames[1]->GetPts() <= _next_pts)
		{
			_frames.erase(_frames.begin());
			continue;
		}

		_curr_pts = _next_pts;

		// Increase the next PTS
		_next_pts++;

		// Skip Frame
		if ((_skip_frames > 0) && (stat_output_frame_count++ % (_skip_frames + 1) != 0))
		{
			continue;
		}

		// Changed from Framerate PTS to Timebase PTS
		int64_t curr_timebase_pts = av_rescale_q_rnd(_curr_pts,
											   av_inv_q(av_d2q(_output_framerate, INT_MAX)),
											   (AVRational){_input_timebase.GetNum(), _input_timebase.GetDen()},
											   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

		// Calculate the PTS of the next frame considering the Skip Frame. 
		// Purpose of calculating the Duration of the current frame
		int64_t next_timebase_pts = av_rescale_q_rnd(_next_pts + _skip_frames,
											   av_inv_q(av_d2q(_output_framerate, INT_MAX)),
											   (AVRational){_input_timebase.GetNum(), _input_timebase.GetDen()},
											   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));


		auto pop_frame = _frames[0]->CloneFrame();
		pop_frame->SetPts(curr_timebase_pts);

		int64_t duration = next_timebase_pts - curr_timebase_pts;
		pop_frame->SetDuration(duration);

		// logtd("Pop Frame. PTS(%lld), Next.PTS(%lld) -> PTS(%lld), Duration(%lld)", _frames[0]->GetPts(), _next_pts, pop_frame->GetPts(), pop_frame->GetDuration());

		return pop_frame;
	}

	return nullptr;
}

ov::String FilterFps::GetStatsString()
{
	ov::String stat;
	stat.Format("InputFrameCount: %lld\n", stat_input_frame_count);
	stat.Append("OutputFrameCount: %lld\n", stat_output_frame_count);
	stat.Append("SkipFrameCount: %lld\n", stat_skip_frame_count);
	stat.Append("DuplicateFrameCount : %lld\n", stat_duplicate_frame_count);
	stat.Append("DiscardFrameCount : %lld\n", stat_discard_frame_count);

	return stat;
}

ov::String FilterFps::GetInfoString()
{
	ov::String info;
	info.Append(ov::String::FormatString("Input Timebase: %d/%d, ", _input_timebase.GetNum(), _input_timebase.GetDen()));
	info.Append(ov::String::FormatString("Input FrameRate: %.2f, ", _input_framerate));
	info.Append(ov::String::FormatString("Output FrameRate: %.2f, ", _output_framerate));
	info.Append(ov::String::FormatString("Skip Frames: %d", _skip_frames));

	return info;
}