//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_profile_template.h"
namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct VideoProfile : public VideoProfileTemplate
				{
				protected:
					std::vector<VideoProfileTemplate> _variants;

				public:
					VideoProfile() = default;
					VideoProfile(VideoProfileTemplate profile)
						: VideoProfileTemplate(profile)
					{
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetVariants, _variants);

				protected:
					void MakeList() override
					{
						VideoProfileTemplate::MakeList();

						Register<Optional>({"Variant", "variants"}, &_variants, [=]() -> std::shared_ptr<ConfigError> {
							for (auto &variant : _variants)
							{
								if (variant.GetName().IsEmpty())
								{
									return CreateConfigErrorPtr("Variant name must be specified");
								}

								SET_IF_NOT_PARSED(IsBypass, SetBypass, _bypass);
								SET_IF_NOT_PARSED(IsActive, SetActive, _active);
								SET_IF_NOT_PARSED(GetCodec, SetCodec, _codec);
								SET_IF_NOT_PARSED(GetBitrate, SetBitrate, _bitrate);
								SET_IF_NOT_PARSED(GetBitrateString, SetBitrateString, _bitrate_string);
								SET_IF_NOT_PARSED(GetWidth, SetWidth, _width);
								SET_IF_NOT_PARSED(GetHeight, SetHeight, _height);
								SET_IF_NOT_PARSED(GetFramerate, SetFramerate, _framerate);
								SET_IF_NOT_PARSED(GetScale, SetScale, _scale);
								SET_IF_NOT_PARSED(GetPreset, SetPreset, _preset);
								SET_IF_NOT_PARSED(GetThreadCount, SetThreadCount, _thread_count);
							}

							return nullptr;
						});
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg