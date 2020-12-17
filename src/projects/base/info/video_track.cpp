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
	:_framerate(0),
	_video_timescale(0),
	_width(0),
	_height(0),
	_format(0)
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

