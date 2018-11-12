//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "audio_encode_options.h"

namespace cfg
{
	struct AudioProfile : public Item
	{
		bool IsBypass() const
		{
			return _bypass;
		}

		const AudioEncodeOptions *GetAudioEncodeOptions() const
		{
			return IsParsed(&_audio_encode_options) ? &_audio_encode_options : nullptr;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Bypass", &_bypass);
			RegisterValue<Optional>("AudioEncodeOptions", &_audio_encode_options);
		}

		bool _bypass = false;
		AudioEncodeOptions _audio_encode_options;
	};
}