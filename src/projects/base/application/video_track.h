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

protected:
	double _framerate;
	int32_t _width;
	int32_t _height;
};
