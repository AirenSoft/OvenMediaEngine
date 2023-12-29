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
		Rendition(const ov::String &name, const ov::String &video_variant_name, const ov::String &audio_variant_name)
			: _name(name), _video_variant_name(video_variant_name), _audio_variant_name(audio_variant_name)
		{

		}

		// Getter
		ov::String GetName() const
		{
			return _name;
		}

		ov::String GetVideoVariantName() const
		{
			return _video_variant_name;
		}

		ov::String GetAudioVariantName() const
		{
			return _audio_variant_name;
		}

		// equals operator
		bool operator==(const Rendition &rhs) const
		{
			if (_name != rhs._name)
			{
				return false;
			}

			if (_video_variant_name != rhs._video_variant_name)
			{
				return false;
			}

			if (_audio_variant_name != rhs._audio_variant_name)
			{
				return false;
			}

			return true;
		}

		// not equals operator
		bool operator!=(const Rendition &rhs) const
		{
			return !(*this == rhs);
		}

	private:
		ov::String _name;
		ov::String _video_variant_name;
		ov::String _audio_variant_name;
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

		void SetHlsChunklistPathDepth(int depth)
		{
			_hls_chunklist_path_depth = depth;
		}

		int GetHlsChunklistPathDepth() const
		{
			return _hls_chunklist_path_depth;
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

		// equals operator
		bool operator==(const Playlist &rhs) const
		{
			if (_name != rhs._name)
			{
				return false;
			}

			if (_file_name != rhs._file_name)
			{
				return false;
			}

			if (_webrtc_auto_abr != rhs._webrtc_auto_abr)
			{
				return false;
			}

			if (_hls_chunklist_path_depth != rhs._hls_chunklist_path_depth)
			{
				return false;
			}

			if (_renditions.size() != rhs._renditions.size())
			{
				return false;
			}

			for (size_t i = 0; i < _renditions.size(); i++)
			{
				if (*_renditions[i] != *rhs._renditions[i])
				{
					return false;
				}
			}

			return true;
		}

		// not equals operator
		bool operator!=(const Playlist &rhs) const
		{
			return !(*this == rhs);
		}

	private:
		ov::String _name;
		ov::String _file_name;

		bool _webrtc_auto_abr = false;
		int _hls_chunklist_path_depth = -1;

		std::vector<std::shared_ptr<Rendition>> _renditions;
	};
} // namespace info