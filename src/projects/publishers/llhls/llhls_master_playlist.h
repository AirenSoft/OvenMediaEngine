//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>

class LLHlsMasterPlaylist
{
public:
	struct MediaInfo
	{
		enum class Type : uint8_t
		{
			Video, Audio, Subtitle, ClosedCaptions
		};

		static MediaInfo &InvalidMediaInfo()
		{
			static MediaInfo info;
			return info;
		}

		ov::String TypeString() const
		{
			switch (_type)
			{
			case Type::Video:
				return "VIDEO";
			case Type::Audio:
				return "AUDIO";
			case Type::Subtitle:
				return "SUBTITLES";
			case Type::ClosedCaptions:
				return "CLOSED-CAPTIONS";
			}

			return "UNKNOWN";
		}

		Type _type; // Required
		ov::String _group_id; // Required
		ov::String _name; // Required
		ov::String _language; // Optional
		bool _auto_select = true; // Optional
		bool _default = false; // Optional
		ov::String _instream_id; // Required for closed captions
		ov::String _assoc_language; // Optional
		ov::String _uri; // Optional 

		// Track
		std::shared_ptr<const MediaTrack> _track; // Required

		// This media info used for master playlist
		bool _used = false;
	};

	struct StreamInfo
	{
		uint32_t _program_id = 1;
		ov::String _uri; // Required
		ov::String _video_group_id; // optional
		ov::String _audio_group_id; // optional
		ov::String _subtitle_group_id; // optional, not supported yet in OME
		ov::String _closed_captions_group_id; // optional, not supported yet in OME
		std::shared_ptr<const MediaTrack> _track; // Required
	};

	bool AddGroupMedia(const MediaInfo &media_info);
	bool AddStreamInfo(const StreamInfo &stream_info);

	ov::String ToString() const;
	std::shared_ptr<const ov::Data> ToGzipData() const;

private:
	bool SetActiveMediaGroup(const ov::String &group_id);
	const MediaInfo &GetDefaultMediaInfo(const ov::String &group_id) const;

	// Cache
	mutable ov::String _playlist_cache;
	mutable bool _need_playlist_updated = true;

	mutable std::shared_ptr<ov::Data> _gzipped_playlist_cache;
	mutable bool _need_gzipped_playlist_updated = true;

	// Group ID : MediaInfo
	std::map<ov::String, std::vector<MediaInfo>>	_media_infos;
	std::vector<StreamInfo> _stream_infos;
};