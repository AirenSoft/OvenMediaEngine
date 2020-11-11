//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct AudioProfile : public Item
				{
				protected:
					bool _bypass = false;
					bool _active = true;
					ov::String _codec;
					int _bitrate = 0;
					ov::String _bitrate_string;
					int _samplerate = 0;
					int _channel = 0;

				public:
					CFG_DECLARE_GETTER_OF(IsBypass, _bypass)
					CFG_DECLARE_GETTER_OF(IsActive, _active)
					CFG_DECLARE_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_GETTER_OF(GetBitrate, _bitrate)
					CFG_DECLARE_GETTER_OF(GetBitrateString, _bitrate_string)
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
						RegisterValue<CondOptional>(
							"Bitrate", &_bitrate_string,
							[this]() -> bool {
								// <Bitrate> is an option when _bypass is true
								return _bypass;
							},
							[this]() -> bool {
								auto bitrate_string = _bitrate_string.UpperCaseString();
								int multiplier = 1;
								if (bitrate_string.HasSuffix("K"))
								{
									multiplier = 1024;
								}
								else if (bitrate_string.HasSuffix("M"))
								{
									multiplier = 1024 * 1024;
								}

								_bitrate = static_cast<int>(ov::Converter::ToFloat(bitrate_string) * multiplier);

								return (_bitrate > 0);
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
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg