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
					double _framerate = 0.0;

				public:
					CFG_DECLARE_REF_GETTER_OF(IsBypass, _bypass)
					CFG_DECLARE_REF_GETTER_OF(IsActive, _active)
					CFG_DECLARE_REF_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_REF_GETTER_OF(GetScale, _scale)
					CFG_DECLARE_REF_GETTER_OF(GetWidth, _width)
					CFG_DECLARE_REF_GETTER_OF(GetHeight, _height)
					CFG_DECLARE_REF_GETTER_OF(GetBitrate, _bitrate)
					CFG_DECLARE_REF_GETTER_OF(GetBitrateString, _bitrate_string)
					CFG_DECLARE_REF_GETTER_OF(GetFramerate, _framerate)

				protected:
					void MakeList() override
					{
						Register<Optional>("Bypass", &_bypass);

						Register<Optional>("Active", &_active);
						Register<Optional>("Codec", &_codec, [=]() -> std::shared_ptr<ConfigError> {
							// <Codec> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigError("Codec must be specified when bypass is false");
						});
						Register<Optional>("Scale", &_scale);
						Register<Optional>("Width", &_width, [=]() -> std::shared_ptr<ConfigError> {
							// <Width> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigError("Width must be specified when bypass is false");
						});
						Register<Optional>("Height", &_height, [=]() -> std::shared_ptr<ConfigError> {
							// <Height> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigError("Height must be specified when bypass is false");
						});
						Register<Optional>(
							"Bitrate", &_bitrate_string,
							[=]() -> std::shared_ptr<ConfigError> {
								// <Bitrate> is an option when _bypass is true
								return (_bypass) ? nullptr : CreateConfigError("Bitrate must be specified when bypass is false");
							},
							[=]() -> std::shared_ptr<ConfigError> {
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

								return (_bitrate > 0) ? nullptr : CreateConfigError("Bitrate must be greater than 0");
							});
						Register<Optional>("Framerate", &_framerate, [=]() -> std::shared_ptr<ConfigError> {
							// <Framerate> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigError("Framerate must be specified when bypass is false");
						});
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg