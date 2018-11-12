//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct AudioProfile : public Item
	{
		bool IsBypass() const
		{
			return _bypass;
		}

		bool IsActive() const
		{
			return _active;
		}

		ov::String GetCodec() const
		{
			return _codec;
		}

		ov::String GetBitrate() const
		{
			return _bitrate;
		}

		int GetSamplerate() const
		{
			return _samplerate;
		}

		int GetChannel() const
		{
			return _channel;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Bypass", &_bypass);
			RegisterValue<Optional>("Active", &_active);
			RegisterValue("Codec", &_codec);
			RegisterValue("Bitrate", &_bitrate);
			RegisterValue("Samplerate", &_samplerate);
			RegisterValue("Channel", &_channel);
		}

		bool _bypass = false;
		bool _active = true;
		ov::String _codec;
		ov::String _bitrate;
		int _samplerate = 0;
		int _channel = 0;
	};
}