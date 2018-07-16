//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_video_info.h"
#include "transcode_audio_info.h"

#include "base/common_types.h"

class TranscodeEncodeInfo
{
public:
	TranscodeEncodeInfo();
	virtual ~TranscodeEncodeInfo();

	const bool GetActive() const noexcept;
	void SetActive(bool active);

	const ov::String GetName() const noexcept;
	void SetName(ov::String name);

	const ov::String GetStreamName() const noexcept;
	void SetStreamName(ov::String stream_name);

	const ov::String GetContainer() const noexcept;
	void SetContainer(ov::String container);

	std::shared_ptr<const TranscodeVideoInfo> GetVideo() const noexcept;
	void SetVideo(std::shared_ptr<TranscodeVideoInfo> video);

	std::shared_ptr<const TranscodeAudioInfo> GetAudio() const noexcept;
	void SetAudio(std::shared_ptr<TranscodeAudioInfo> audio);

	ov::String ToString() const;

private:
	bool _active;
	ov::String _name;
	ov::String _stream_name;
	ov::String _container;

	std::shared_ptr<TranscodeVideoInfo> _video;
	std::shared_ptr<TranscodeAudioInfo> _audio;
};
