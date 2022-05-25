//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rendition.h"
#include "encodes/encodes.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct Renditions : public Item
				{
				protected:
					std::vector<Rendition> _renditions;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetRenditionList, _renditions);

					bool SetEncodes(const Encodes &encodes)
					{
						bool video_found = false;
						bool audio_found = false;
						for (auto &rendition : _renditions)
						{
							video_found = false;
							audio_found = false;

							bool video_enabled = false;
							auto video_name = rendition.GetVideoName(&video_enabled);
							
							if (video_enabled)
							{
								for (const auto &encode : encodes.GetVideoProfileList())
								{
									if (video_name == encode.GetName())
									{
										rendition.SetVideoProfile(encode);
										video_found = true;
									}
								}
							}

							bool audio_enabled = false;
							auto audio_name = rendition.GetAudioName(&audio_enabled);

							if (audio_enabled)
							{
								for (const auto &encode : encodes.GetAudioProfileList())
								{
									if (audio_name == encode.GetName())
									{
										rendition.SetAudioProfile(encode);
										audio_found = true;
									}
								}
							}

							bool result = true;
							if (!video_found && video_enabled)
							{
								loge("Config", "[Renditions] Video profile not found: %s", video_name.CStr());
								result = false;
							}

							if (!audio_found && audio_enabled)
							{
								loge("Config", "[Renditions] Audio profile not found: %s", audio_name.CStr());
								result = false;
							}

							if (result == false)
							{
								return false;
							}
						}

						return true;
					}

				protected:
					void MakeList() override
					{
						Register<Optional>({"Rendition", "renditions"}, &_renditions);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg