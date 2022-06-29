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

namespace info
{

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

	class Playlist
	{
	public:
		Playlist(const ov::String &name, const ov::String &file_name)
		{
			_name = name;
			_file_name = file_name;
		}
		~Playlist() = default;

		void SetWebRtcAutoAbr(bool enabled)
		{
			_webrtc_auto_abr = enabled;
		}

		bool IsWebRtcAutoAbr() const
		{
			return _webrtc_auto_abr;
		}

		// Append Rendition
		void AddRendition(const std::shared_ptr<Rendition> &rendition)
		{
			_renditions.push_back(rendition);
		}

		// Getter
		ov::String GetName() const
		{
			return _name;
		}

		ov::String GetFileName() const
		{
			return _file_name;
		}

		// Get Rendition List
		const std::vector<std::shared_ptr<Rendition>> &GetRenditionList() const
		{
			return _renditions;
		}

	private:
		ov::String _name;
		ov::String _file_name;

		bool _webrtc_auto_abr = false;

		std::vector<std::shared_ptr<Rendition>> _renditions;
	};
} // namespace info