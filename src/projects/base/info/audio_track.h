//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/bitstream/aac/aac_specific_config.h>
#include "base/common_types.h"


class AudioTrack
{
public:
	AudioTrack();
	
	void SetSampleRate(int32_t samplerate);
	int32_t GetSampleRate() const;
	
	void SetAudioTimestampScale(double scale);
	double GetAudioTimestampScale() const;

	cmn::AudioSample &GetSample();
	const cmn::AudioSample &GetSample() const;
	
	void SetChannel(cmn::AudioChannel channel);
	cmn::AudioChannel &GetChannel();
	const cmn::AudioChannel &GetChannel() const;
	
	// Codec-specific data prepared in advance for performance
	std::shared_ptr<AACSpecificConfig> GetAacConfig() const;
	void SetAacConfig(const std::shared_ptr<AACSpecificConfig> &config);

protected:
	// sample format, sample rate
	cmn::AudioSample _sample;

	// channel layout
	cmn::AudioChannel _channel_layout;

	// time_scale
	double _audio_timescale;

	std::shared_ptr<AACSpecificConfig> _aac_config = nullptr;
};