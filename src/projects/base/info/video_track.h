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
	VideoTrack();

	void SetFrameRate(double framerate);
	double GetFrameRate() const;

	void SetWidth(int32_t width);
	int32_t GetWidth() const;

	void SetHeight(int32_t height);
	int32_t GetHeight() const;

	void SetFormat(int32_t format);
	int32_t GetFormat() const;

	void SetVideoTimestampScale(double scale);
	double GetVideoTimestampScale() const;

protected:
	double _framerate;
	double _video_timescale;
	int32_t _width;
	int32_t _height;
	int32_t _format;
};
