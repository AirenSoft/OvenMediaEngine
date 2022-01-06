//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "video_track.h"

VideoTrack::VideoTrack()
	: _framerate(0),
	  _video_timescale(0),
	  _width(0),
	  _height(0),
	  _format(0),
	  _preset(""),
	  _has_bframe(false),
	  _thread_count(0)
{
}

void VideoTrack::SetFrameRate(double framerate)
{
	_framerate = framerate;
}

double VideoTrack::GetFrameRate() const
{
	return _framerate;
}

void VideoTrack::SetEstimateFrameRate(double framerate)
{
	_estimate_framerate = framerate;
}

double VideoTrack::GetEsimateFrameRate() const
{
	return _estimate_framerate;
}

void VideoTrack::SetWidth(int32_t width)
{
	_width = width;
}

int32_t VideoTrack::GetWidth() const
{
	return _width;
}

void VideoTrack::SetHeight(int32_t height)
{
	_height = height;
}

int32_t VideoTrack::GetHeight() const
{
	return _height;
}

void VideoTrack::SetFormat(int32_t format)
{
	_format = format;
}

int32_t VideoTrack::GetFormat() const
{
	return _format;
}

void VideoTrack::SetVideoTimestampScale(double scale)
{
	_video_timescale = scale;
}

double VideoTrack::GetVideoTimestampScale() const
{
	return _video_timescale;
}

std::shared_ptr<ov::Data> VideoTrack::GetH264SpsPpsAnnexBFormat() const
{
	return _h264_sps_pps_annexb_data;
}

const FragmentationHeader& VideoTrack::GetH264SpsPpsAnnexBFragmentHeader() const
{
	return _h264_sps_pps_annexb_fragment_header;
}

void VideoTrack::SetH264SpsPpsAnnexBFormat(const std::shared_ptr<ov::Data>& data, const FragmentationHeader& header)
{
	_h264_sps_pps_annexb_data = data;
	_h264_sps_pps_annexb_fragment_header = header;
}

void VideoTrack::SetPreset(ov::String preset)
{
	_preset = preset;
}

ov::String VideoTrack::GetPreset() const
{
	return _preset;
}

void VideoTrack::SetBframes(bool has_bframe)
{
	_has_bframe = has_bframe;
}

bool VideoTrack::HasBframes()
{
	return _has_bframe;
}

void VideoTrack::SetThreadCount(int thread_count)
{
	_thread_count = thread_count;
}

int VideoTrack::GetThreadCount() 
{
	return _thread_count;
}
