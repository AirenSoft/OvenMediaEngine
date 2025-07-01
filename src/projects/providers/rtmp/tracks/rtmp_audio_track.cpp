//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./rtmp_audio_track.h"

#include "../rtmp_provider_private.h"

namespace pvd::rtmp
{
	std::shared_ptr<MediaTrack> RtmpAudioTrack::CreateMediaTrack(
		const modules::flv::ParserCommon &parser,
		const std::shared_ptr<const modules::flv::CommonData> &data)
	{
		if (std::dynamic_pointer_cast<const modules::flv::AudioData>(data) == nullptr)
		{
			OV_ASSERT2(false);
			return nullptr;
		}

		return RtmpTrack::CreateMediaTrack(parser, data);
	}
}  // namespace pvd::rtmp
