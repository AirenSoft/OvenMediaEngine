//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "origin.h"
#include "web_console.h"
#include "decode.h"
#include "encodes.h"
#include "streams.h"
#include "providers.h"
#include "publishers.h"

namespace cfg
{
	enum class ApplicationType
	{
		Unknown,
		Live,
		Vod,
		LiveEdge,
		VodEdge
	};

	struct Application : public Item
	{
		ov::String GetName() const
		{
			return _name;
		}

		ApplicationType GetType() const
		{
			if(_type == "live")
			{
				return ApplicationType::Live;
			}
			else if(_type == "liveedge")
			{
				return ApplicationType::LiveEdge;
			}
			else if(_type == "vod")
			{
				return ApplicationType::Vod;
			}
			else if(_type == "vodedge")
			{
				return ApplicationType::VodEdge;
			}

			return ApplicationType::Unknown;
		}

		ov::String GetTypeName() const
		{
			return _type;
		}

		const Origin &GetOrigin() const
		{
			return _origin;
		}

		const WebConsole &GetWebConsole() const
		{
			return _web_console;
		}

		const Decode &GetDecode() const
		{
			return _decode;
		}

		const std::vector<Encode> &GetEncodes() const
		{
			return _encodes.GetEncodes();
		}

		bool HasEncodeWithCodec(const ov::String &profile_name,
			cfg::StreamProfileUse use,
			const std::vector<ov::String> &video_codecs, const
			std::vector<ov::String> &audio_codecs) const
		{
			for(const auto &encode : GetEncodes())
			{
				if(encode.IsActive() && encode.GetName() == profile_name)
				{
					// video codec
					if ((video_codecs.empty() == false)
						&& (use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::VideoOnly))
					{
						const auto &video_profiles = encode.GetVideoProfiles();
						return std::find_if(video_profiles.begin(), video_profiles.end(), [video_codecs](const auto &video_profile)
						{
							return std::find(video_codecs.begin(), video_codecs.end(), video_profile.GetCodec()) != video_codecs.end();
						}) != video_profiles.end();
					}

					// audio codec
					if ((audio_codecs.empty() == false)
						&& (use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::AudioOnly))
					{
						const auto &audio_profiles = encode.GetAudioProfiles();
						return std::find_if(audio_profiles.begin(), audio_profiles.end(), [audio_codecs](const auto &audio_profile)
						{
							return std::find(audio_codecs.begin(), audio_codecs.end(), audio_profile.GetCodec()) != audio_codecs.end();
						}) != audio_profiles.end();
					}
				}
			}
			return false;
		}
	
		const std::vector<Stream> &GetStreamList() const
		{
			return _streams.GetStreamList();
		}

		const Providers &GetProviders() const
		{
			return _providers;
		}

		const Publishers &GetPublishers() const
		{
			return _publishers;
		}

		const int GetThreadCount() const
		{
			return _publishers.GetThreadCount();
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_type);
			RegisterValue<Optional>("Origin", &_origin);
			RegisterValue<Optional>("WebConsole", &_web_console);
			RegisterValue<Optional>("Decode", &_decode);
			RegisterValue<Optional>("Encodes", &_encodes);
			RegisterValue<Optional>("Streams", &_streams);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		Origin _origin;
		WebConsole _web_console;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}