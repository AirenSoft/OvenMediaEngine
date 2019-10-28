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
			RegisterValue<CondOptional>("Codec", &_codec, [this]() -> bool {
				// <Codec> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Bitrate", &_bitrate, [this]() -> bool {
				// <Bitrate> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Samplerate", &_samplerate, [this]() -> bool {
				// <Samplerate> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Channel", &_channel, [this]() -> bool {
				// <Channel> is an option when _bypass is true
				return _bypass;
			});
		}

		bool _bypass = false;
		bool _active = true;
		ov::String _codec;
		ov::String _bitrate;
		int _samplerate = 0;
		int _channel = 0;
	};
}