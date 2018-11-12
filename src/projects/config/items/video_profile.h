//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_encode_options.h"

namespace cfg
{
	struct VideoProfile : public Item
	{
		bool GetBypass() const
		{
			return _bypass;
		}

		const VideoEncodeOptions *GetVideoEncodeOptions() const
		{
			return IsParsed(&_video_encode_options) ? &_video_encode_options : nullptr;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Bypass", &_bypass);
			RegisterValue<Optional>("VideoEncodeOptions", &_video_encode_options);
		}

		bool _bypass = false;
		VideoEncodeOptions _video_encode_options;
	};
}