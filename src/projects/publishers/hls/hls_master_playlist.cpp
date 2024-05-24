//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_master_playlist.h"

HlsMasterPlaylist::HlsMasterPlaylist(const ov::String &playlist_file_name, const Config &config)
	: _playlist_file_name(playlist_file_name)
	, _config(config)
{
}

bool HlsMasterPlaylist::SetDefaultOption(bool rewind)
{
	_default_option_rewind = rewind;
	return true;
}

void HlsMasterPlaylist::AddMediaPlaylist(const std::shared_ptr<HlsMediaPlaylist> &media_playlist)
{
	std::lock_guard<std::shared_mutex> lock(_media_playlists_mutex);
	_media_playlists.emplace(media_playlist->GetVariantName(), media_playlist);
}

ov::String HlsMasterPlaylist::ToString(bool rewind) const
{
	std::shared_lock<std::shared_mutex> lock(_media_playlists_mutex);

	ov::String result = "#EXTM3U\n";

	result += ov::String::FormatString("#EXT-X-VERSION:%d\n", _config.version);

	for (const auto &media_playlist_it : _media_playlists)
	{
		const auto &media_playlist = media_playlist_it.second;

		if (media_playlist->HasVideo())
		{			
			result += ov::String::FormatString("#EXT-X-STREAM-INF:BANDWIDTH=%d,AVERAGE-BANDWIDTH=%d,RESOLUTION=%s,FRAME-RATE=%.3f,CODECS=\"%s\"\n",
											media_playlist->GetBitrates(),
											media_playlist->GetAverageBitrate(),
											media_playlist->GetResolutionString().CStr(),
											media_playlist->GetFramerate(),	
											media_playlist->GetCodecsString().CStr());
											
		}
		else
		{
			result += ov::String::FormatString("#EXT-X-STREAM-INF:BANDWIDTH=%d,AVERAGE-BANDWIDTH=%d,CODECS=\"%s\"\n",
											media_playlist->GetBitrates(),
											media_playlist->GetAverageBitrate(),
											media_playlist->GetCodecsString().CStr());
		}

		result += ov::String::FormatString("%s", media_playlist->GetPlaylistFileName().CStr());

		if (rewind != _default_option_rewind)
		{
			result += ov::String::FormatString("?_HLS_rewind=%s", rewind ? "YES" : "NO");
		}

		result += "\n";
	}

	return result;
}