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
				struct AudioProfileTemplate : public Item
				{
				protected:
					ov::String _name;
					bool _bypass = false;
					bool _active = true;
					ov::String _codec;
					int _bitrate = 0;
					ov::String _bitrate_string;
					int _samplerate = 0;
					int _channel = 0;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsBypass, _bypass)
					CFG_DECLARE_CONST_REF_GETTER_OF(IsActive, _active)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetBitrate, _bitrate)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetBitrateString, _bitrate_string)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSamplerate, _samplerate)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetChannel, _channel)

					void SetName(const ov::String &name){_name = name;}
					void SetBypass(bool bypass){_bypass = bypass;}
					void SetActive(bool active){_active = active;}
					void SetCodec(const ov::String &codec){_codec = codec;}
					void SetBitrate(int bitrate){_bitrate = bitrate;}
					void SetBitrateString(const ov::String &bitrate_string){_bitrate_string = bitrate_string;}
					void SetSamplerate(int samplerate){_samplerate = samplerate;}
					void SetChannel(int channel){_channel = channel;}

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
#if 1
						Register<Optional>("Samplerate", &_samplerate);
						Register<Optional>("Channel", &_channel);
#else
						Register<Optional>("Samplerate", &_samplerate, [=]() -> std::shared_ptr<ConfigError> {
							// <Samplerate> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigErrorPtr("Samplerate must be specified when bypass is false");
						});
						Register<Optional>("Channel", &_channel, [=]() -> std::shared_ptr<ConfigError> {
							// <Channel> is an option when _bypass is true
							return (_bypass) ? nullptr : CreateConfigErrorPtr("Channel must be specified when bypass is false");
						});
#endif
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg