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
				struct VideoProfileTemplate : public Item
				{
				protected:
					ov::String _name;
					bool _bypass = false;
					bool _active = true;
					ov::String _codec;
					ov::String _scale;
					int _width = 0;
					int _height = 0;
					int _bitrate = 0;
					ov::String _bitrate_string;
					double _framerate = 0.0;
					ov::String _preset;
					int _thread_count = 0;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsBypass, _bypass)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsActive, _active)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetScale, _scale)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetWidth, _width)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetHeight, _height)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetBitrate, _bitrate)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetBitrateString, _bitrate_string)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetFramerate, _framerate)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetPreset, _preset)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetThreadCount, _thread_count)

					void SetBypass(bool bypass){_bypass = bypass;}
					void SetActive(bool active){_active = active;}
					void SetCodec(const ov::String &codec){_codec = codec;}
					void SetScale(const ov::String &scale){_scale = scale;}
					void SetWidth(int width){_width = width;}
					void SetHeight(int height){_height = height;}
					void SetBitrate(int bitrate){_bitrate = bitrate;}
					void SetBitrateString(const ov::String &bitrate_string){_bitrate_string = bitrate_string;}
					void SetFramerate(double framerate){_framerate = framerate;}
					void SetPreset(const ov::String &preset){_preset = preset;}
					void SetThreadCount(int thread_count){_thread_count = thread_count;}
					

				protected:
					void MakeList() override
					{
						Register<Optional>("Name", &_name);
						Register<Optional>("Bypass", &_bypass);
						Register<Optional>("Active", &_active);
						Register<Optional>("Codec", &_codec, [=]() -> std::shared_ptr<ConfigError> {
							// <Codec> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigErrorPtr("Codec must be specified when bypass is false");
						});
						Register<Optional>(
							"Bitrate", &_bitrate_string,
							[=]() -> std::shared_ptr<ConfigError> {
								// <Bitrate> is an option when _bypass is true
								return (_bypass) ? nullptr : CreateConfigErrorPtr("Bitrate must be specified when bypass is false");
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

								return (_bitrate > 0) ? nullptr : CreateConfigErrorPtr("Bitrate must be greater than 0");
							});
						Register<Optional>("Scale", &_scale);
						Register<Optional>("Width", &_width);
						Register<Optional>("Height", &_height);
						Register<Optional>("Framerate", &_framerate);
						Register<Optional>("Preset", &_preset);
						Register<Optional>("ThreadCount", &_thread_count);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg