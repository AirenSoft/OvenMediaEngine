//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../common_types.h"

class AudioTrack
{
public:
	void SetSampleRate(int32_t samplerate);
	int32_t GetSampleRate();

	MediaCommonType::AudioSample::Format GetSampleFormat();
	MediaCommonType::AudioChannel::Layout GetChannelLayout();

	MediaCommonType::AudioSample &GetSample();
	MediaCommonType::AudioChannel &GetChannel();

protected:
	// sample format, sample rate
	MediaCommonType::AudioSample _sample;

	// channel layout
	MediaCommonType::AudioChannel _channel_layout;

};