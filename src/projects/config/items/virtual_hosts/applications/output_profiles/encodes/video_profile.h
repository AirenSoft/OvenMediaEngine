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
				struct VideoProfile : public Item
				{
				protected:
					bool _bypass = false;
					bool _active = true;
					ov::String _codec;
					ov::String _scale;
					int _width = 0;
					int _height = 0;
					int _bitrate = 0;
					ov::String _bitrate_string;
					float _framerate = 0.0f;

				public:
					CFG_DECLARE_GETTER_OF(IsBypass, _bypass)
					CFG_DECLARE_GETTER_OF(IsActive, _active)
					CFG_DECLARE_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_GETTER_OF(GetScale, _scale)
					CFG_DECLARE_GETTER_OF(GetWidth, _width)
					CFG_DECLARE_GETTER_OF(GetHeight, _height)
					CFG_DECLARE_GETTER_OF(GetBitrate, _bitrate)
					CFG_DECLARE_GETTER_OF(GetBitrateString, _bitrate_string)
					CFG_DECLARE_GETTER_OF(GetFramerate, _framerate)

				protected:
					void MakeParseList() override
					{
						RegisterValue<Optional>("Bypass", &_bypass);

						RegisterValue<Optional>("Active", &_active);
						RegisterValue<CondOptional>("Codec", &_codec, [this]() -> bool {
							// <Codec> is an option when _bypass is true
							return _bypass;
						});
						RegisterValue<Optional>("Scale", &_scale);
						RegisterValue<CondOptional>("Width", &_width, [this]() -> bool {
							// <Width> is an option when _bypass is true
							return _bypass;
						});
						RegisterValue<CondOptional>("Height", &_height, [this]() -> bool {
							// <Height> is an option when _bypass is true
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
						RegisterValue<CondOptional>("Framerate", &_framerate, [this]() -> bool {
							// <Framerate> is an option when _bypass is true
							return _bypass;
						});
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg