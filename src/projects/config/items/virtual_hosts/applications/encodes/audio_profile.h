//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct AudioProfile : public Item
	{
		CFG_DECLARE_GETTER_OF(IsBypass, _bypass)
		CFG_DECLARE_GETTER_OF(IsActive, _active)
		CFG_DECLARE_GETTER_OF(GetCodec, _codec)
		CFG_DECLARE_GETTER_OF(GetBitrate, _bitrate)
		CFG_DECLARE_GETTER_OF(GetSamplerate, _samplerate)
		CFG_DECLARE_GETTER_OF(GetChannel, _channel)

	protected:
		void MakeParseList() override
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
}  // namespace cfg