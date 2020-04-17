//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "video_track.h"

void VideoTrack::SetFrameRate(double framerate)
{
	_framerate = framerate;
}

double VideoTrack::GetFrameRate()
{
	return _framerate;
}

void VideoTrack::SetWidth(int32_t width)
{
	_width = width;
}

int32_t VideoTrack::GetWidth()
{
	return _width;
}

void VideoTrack::SetHeight(int32_t height)
{
	_height = height;
}

int32_t VideoTrack::GetHeight()
{
	return _height;
}

void VideoTrack::SetFormat(int32_t format)
{
	_format = format;
}

int32_t VideoTrack::GetFormat()
{
	return _format;
}

void VideoTrack::SetVideoTimestampScale(double scale)
{
	_video_timescale = scale;
}

double VideoTrack::GetVideoTimestampScale()
{
	return _video_timescale;
}

