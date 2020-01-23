//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "decode/decode.h"
#include "encodes/encodes.h"
#include "origin.h"
#include "providers/providers.h"
#include "publishers/publishers.h"
#include "streams/streams.h"
#include "web_console/web_console.h"

namespace cfg
{
	enum class ApplicationType
	{
		Unknown,
		Live,
		VoD
	};

	struct Application : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)
		CFG_DECLARE_REF_GETTER_OF(GetType, _type_value)

		CFG_DECLARE_REF_GETTER_OF(GetOrigin, _origin)
		CFG_DECLARE_REF_GETTER_OF(GetDecode, _decode)
		CFG_DECLARE_REF_GETTER_OF(GetEncodeList, _encodes.GetEncodeList())
		CFG_DECLARE_REF_GETTER_OF(GetStreamList, _streams.GetStreamList())
		CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
		CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)
		CFG_DECLARE_GETTER_OF(GetThreadCount, _publishers.GetThreadCount())

		bool HasEncodeWithCodec(const ov::String &profile_name,
								cfg::StreamProfileUse use,
								const std::vector<ov::String> &video_codecs, const std::vector<ov::String> &audio_codecs) const
		{
			for (const auto &encode : GetEncodeList())
			{
				if (encode.IsActive() && encode.GetName() == profile_name)
				{
					// video codec
					if ((video_codecs.empty() == false) && (use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::VideoOnly))
					{
						const auto &video_profiles = encode.GetVideoProfileList();
						return std::find_if(video_profiles.begin(), video_profiles.end(), [video_codecs](const auto &video_profile) {
								   return std::find(video_codecs.begin(), video_codecs.end(), video_profile.GetCodec()) != video_codecs.end();
							   }) != video_profiles.end();
					}

					// audio codec
					if ((audio_codecs.empty() == false) && (use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::AudioOnly))
					{
						const auto &audio_profiles = encode.GetAudioProfileList();
						return std::find_if(audio_profiles.begin(), audio_profiles.end(), [audio_codecs](const auto &audio_profile) {
								   return std::find(audio_codecs.begin(), audio_codecs.end(), audio_profile.GetCodec()) != audio_codecs.end();
							   }) != audio_profiles.end();
					}
				}
			}
			return false;
		}

	protected:
		void MakeParseList() override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_type, nullptr, [this]() -> bool {
				if (_type == "live")
				{
					_type_value = ApplicationType::Live;
				}
				else if (_type == "vod")
				{
					_type_value = ApplicationType::VoD;
				}
				else
				{
					return false;
				}

				return true;
			});

			RegisterValue<Optional>("Origin", &_origin);
			RegisterValue<Optional>("Decode", &_decode);
			RegisterValue<Optional>("Encodes", &_encodes);
			RegisterValue<Optional>("Streams", &_streams);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		ApplicationType _type_value;

		Origin _origin;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}  // namespace cfg