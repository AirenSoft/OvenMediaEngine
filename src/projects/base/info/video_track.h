//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class VideoTrack
{
public:
	void SetFrameRate(double framerate);
	double GetFrameRate();

	void SetWidth(int32_t width);
	int32_t GetWidth();

	void SetHeight(int32_t height);
	int32_t GetHeight();

	void SetFormat(int32_t format);
	int32_t GetFormat();

	void SetVideoTimestampScale(double scale);
	double GetVideoTimestampScale();

protected:
	double _framerate;
	double _video_timescale;
	int32_t _width;
	int32_t _height;
	int32_t _format;
};
