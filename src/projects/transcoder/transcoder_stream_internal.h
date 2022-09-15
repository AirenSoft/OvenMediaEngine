//==============================================================================
//
//  TranscoderStreamInternal
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/application.h>

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_type.h"

class TranscoderStreamInternal
{
public:
	TranscoderStreamInternal();
	~TranscoderStreamInternal();

public:
	MediaTrackId NewTrackId();
	std::atomic<MediaTrackId> _last_track_index = 0;

	static ov::String GetIdentifiedForVideoProfile(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile);
	static ov::String GetIdentifiedForAudioProfile(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile);
	static ov::String GetIdentifiedForImageProfile(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile);
	static ov::String GetIdentifiedForDataProfile(const uint32_t track_id);
	static cmn::Timebase GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id);
};
