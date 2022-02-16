//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "audio_profile_template.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct AudioProfile : public AudioProfileTemplate
				{
				protected:
					std::vector<AudioProfileTemplate> _variants;

				public:
					AudioProfile() = default;
					AudioProfile(AudioProfileTemplate profile)
						: AudioProfileTemplate(profile)
					{
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetVariants, _variants);

				protected:
					void MakeList() override
					{
						AudioProfileTemplate::MakeList();

						Register<Optional>({"Variant", "variants"}, &_variants, [=]() -> std::shared_ptr<ConfigError> {
							for (auto &variant : _variants)
							{
								if (variant.GetName().IsEmpty())
								{
									return CreateConfigErrorPtr("Variant name must be specified");
								}

								SET_IF_NOT_PARSED(GetCodec, SetCodec, _codec);
								SET_IF_NOT_PARSED(GetBitrate, SetBitrate, _bitrate);
								SET_IF_NOT_PARSED(GetBitrateString, SetBitrateString, _bitrate_string);
								SET_IF_NOT_PARSED(GetSamplerate, SetSamplerate, _samplerate);
								SET_IF_NOT_PARSED(GetChannel, SetChannel, _channel);
							}

							return nullptr;
						});
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg