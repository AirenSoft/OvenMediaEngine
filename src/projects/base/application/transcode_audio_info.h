//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class TranscodeAudioInfo
{
public:
	TranscodeAudioInfo();
	virtual ~TranscodeAudioInfo();

	const bool GetActive() const noexcept;
	void SetActive(bool active);

	const ov::String GetCodec() const noexcept;
	void SetCodec(ov::String codec);

	const int GetBitrate() const noexcept;
	void SetBitrate(int bitrate);

	const int GetSamplerate() const noexcept;
	void SetSamplerate(int samplerate);

	const int GetChannel() const noexcept;
	void SetChannel(int channel);

	ov::String ToString() const;

private:
	bool _active;
	ov::String _codec;
	int _bitrate;
	int _samplerate;
	int _channel;
};
