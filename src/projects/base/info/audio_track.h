//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class AudioTrack
{
public:
	void SetSampleRate(int32_t samplerate);
	int32_t GetSampleRate();
	
	void SetAudioTimestampScale(double scale);
	double GetAudioTimestampScale();

	cmn::AudioSample &GetSample();
	cmn::AudioChannel &GetChannel();
	const cmn::AudioChannel &GetChannel() const;

protected:
	// sample format, sample rate
	cmn::AudioSample _sample;

	// channel layout
	cmn::AudioChannel _channel_layout;

	// time_scale
	double _audio_timescale;

};