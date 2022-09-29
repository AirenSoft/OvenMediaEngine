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
	class Dump
	{
	public:
		Dump(const ov::String &output_path)
		{
			_output_path = output_path;
		}

		void SetEnabled(bool enabled)
		{
			_enabled = enabled;
			if (enabled == true)
			{
				_enabled_time = std::chrono::system_clock::now();
			}
		}

		bool IsEnabled() const
		{
			return _enabled;
		}

		// Get enabled time
		std::chrono::system_clock::time_point GetEnabledTime() const
		{
			return _enabled_time;
		}

		void SetOutputPath(const ov::String &output_path)
		{
			_output_path = output_path;
		}

		const ov::String &GetOutputPath() const
		{
			return _output_path;
		}

		// Playlists
		void AddPlaylist(const ov::String &playlist)
		{
			_playlists.push_back(playlist);
		}

		void SetPlaylists(const std::vector<ov::String> &playlists)
		{
			_playlists = playlists;
		}

		const std::vector<ov::String> &GetPlaylists() const
		{
			return _playlists;
		}

		bool HasFirstSegmentNumber(const int32_t &track_id)
		{
			return _first_segment_numbers.find(track_id) != _first_segment_numbers.end();
		}

		uint32_t GetFirstSegmentNumber(const int32_t &track_id)
		{
			return _first_segment_numbers[track_id];
		}

		void SetFirstSegmentNumber(const int32_t &track_id, const uint32_t &segment_number)
		{
			_first_segment_numbers[track_id] = segment_number;
		}

	private:
		bool _enabled = false;
		std::chrono::system_clock::time_point _enabled_time;
		ov::String _output_path;

		std::map<int32_t, uint32_t> _first_segment_numbers;

		std::vector<ov::String> _playlists;
	};
}