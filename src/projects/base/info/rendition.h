//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class Rendition
{
public:
	Rendition(const ov::String &name, const ov::String &video_track_name, const ov::String &audio_track_name)
		: _name(name), _video_track_name(video_track_name), _audio_track_name(audio_track_name)
	{

	}

	// Getter
	ov::String GetName() const
	{
		return _name;
	}

	ov::String GetVideoTrackName() const
	{
		return _video_track_name;
	}

	ov::String GetAudioTrackName() const
	{
		return _audio_track_name;
	}

private:
	ov::String _name;
	ov::String _video_track_name;
	ov::String _audio_track_name;
};