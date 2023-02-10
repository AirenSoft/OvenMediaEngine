//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "llhls_master_playlist.h"
#include "llhls_private.h"

#include <base/ovlibrary/zip.h>
#include <modules/bitstream/codec_media_type.h>

void LLHlsMasterPlaylist::SetChunkPath(const ov::String &chunk_path)
{
	_chunk_path = chunk_path;
}

bool LLHlsMasterPlaylist::AddMediaCandidateGroup(const std::shared_ptr<const MediaTrackGroup> &track_group, std::function<ov::String(const std::shared_ptr<const MediaTrack> &track)> chunk_uri_generator)
{
	auto first_track = track_group->GetFirstTrack();

	auto new_group = std::make_shared<MediaGroup>();
	new_group->_group_id = track_group->GetName();
	
	// Add media info
	bool first = true;
	for(auto &track : track_group->GetTracks())
	{
		auto new_media_info = std::make_shared<MediaInfo>();
		new_media_info->_group_id = track_group->GetName();
		new_media_info->_type = track->GetMediaType();
		new_media_info->_name = track->GetPublicName();
		new_media_info->_language = track->GetLanguage();
		new_media_info->_default = first;
		new_media_info->_auto_select = first || (track->GetLanguage().IsEmpty() == false);
		new_media_info->_instream_id = "";
		new_media_info->_assoc_language = "";
		new_media_info->_uri = chunk_uri_generator(track);
		new_media_info->_track = track;

		new_group->_media_infos.push_back(new_media_info);
		first = false;
	}

	std::lock_guard<std::shared_mutex> lock(_media_groups_guard);
	_media_groups.emplace(new_group->_group_id, new_group);

	return true;
}

bool LLHlsMasterPlaylist::AddStreamInfo(const ov::String &video_group_id, const ov::String &audio_group_id)
{
	auto new_stream_info = std::make_shared<StreamInfo>();

	// Video/Audio or Video Only
	if (video_group_id.IsEmpty() == false)
	{
		auto video_group = GetMediaGroup(video_group_id);
		if (video_group == nullptr || video_group->_media_infos.size() < 1)
		{
			logte("Could not find valid video group: %s", video_group_id.CStr());
			return false;
		}

		// first media info is used for stream info
		new_stream_info->_media_info = video_group->_media_infos[0];
		auto video_track = new_stream_info->_media_info->_track;
		if (video_track == nullptr)
		{
			logte("Video track is not valid in first media info of video group: %s", video_group_id.CStr());
			return false;
		}

		new_stream_info->_bandwidth += video_track->GetBitrate();
		new_stream_info->_width = video_track->GetWidth();
		new_stream_info->_height = video_track->GetHeight();
		new_stream_info->_framerate = video_track->GetFrameRate();
		new_stream_info->_codecs = CodecMediaType::GetCodecsParameter(video_track);
		
		// Active media group if video group has more than 1 media info
		if (video_group->_media_infos.size() > 1)
		{
			video_group->_used = true;
			new_stream_info->_video_group_id = video_group_id;
		}

		if (audio_group_id.IsEmpty() == false)
		{
			auto audio_group = GetMediaGroup(audio_group_id);
			if (audio_group == nullptr || audio_group->_media_infos.size() < 1)
			{
				logte("Could not find valid audio group: %s", audio_group_id.CStr());
				return false;
			}

			audio_group->_used = true;
			new_stream_info->_audio_group_id = audio_group_id;

			auto audio_track = audio_group->_media_infos[0]->_track;
			if (audio_track == nullptr)
			{
				logte("Audio track is not valid in first media info of audio group: %s", audio_group_id.CStr());
				return false;
			}

			new_stream_info->_bandwidth += audio_track->GetBitrate();
			new_stream_info->_codecs += ov::String::FormatString(",%s", CodecMediaType::GetCodecsParameter(audio_track).CStr());
		}
	}
	// Audio Only 
	else if (audio_group_id.IsEmpty() == false)
	{
		auto audio_group = GetMediaGroup(audio_group_id);
		if (audio_group == nullptr || audio_group->_media_infos.size() < 1)
		{
			logte("Could not find valid audio group: %s", audio_group_id.CStr());
			return false;
		}

		// first media info is used for stream info
		new_stream_info->_media_info = audio_group->_media_infos[0];
		auto audio_track = new_stream_info->_media_info->_track;
		if (audio_track == nullptr)
		{
			logte("Audio track is not valid in first media info of audio group: %s", audio_group_id.CStr());
			return false;
		}

		new_stream_info->_bandwidth += audio_track->GetBitrate();
		new_stream_info->_codecs = CodecMediaType::GetCodecsParameter(audio_track);

		if (audio_group->_media_infos.size() > 1)
		{
			audio_group->_used = true;
			new_stream_info->_audio_group_id = audio_group_id;
		}
	}
	else
	{
		logte("video group id and audio group id are empty");
		return false;
	}

	std::lock_guard<std::shared_mutex> lock(_stream_infos_guard);
	_stream_infos.emplace_back(new_stream_info);

	return true;
}

std::shared_ptr<LLHlsMasterPlaylist::MediaGroup> LLHlsMasterPlaylist::GetMediaGroup(const ov::String &group_id) const
{
	std::shared_lock<std::shared_mutex> lock(_media_groups_guard);

	auto it = _media_groups.find(group_id);
	if (it == _media_groups.end())
	{
		return nullptr;
	}

	return it->second;
}

void LLHlsMasterPlaylist::UpdateCacheForDefaultPlaylist()
{
	ov::String playlist = MakePlaylist("", false);
	{
		// lock 
		std::lock_guard<std::shared_mutex> lock(_cached_default_playlist_guard);
		_cached_default_playlist = playlist;
	}

	{
		// lock 
		std::lock_guard<std::shared_mutex> lock(_cached_default_playlist_gzip_guard);
		_cached_default_playlist_gzip = ov::Zip::CompressGzip(playlist.ToData(false));
	}
}

ov::String LLHlsMasterPlaylist::MakePlaylist(const ov::String &chunk_query_string, bool legacy, bool include_path) const
{
	ov::String playlist(10240);

	playlist.AppendFormat("#EXTM3U\n");
	playlist.AppendFormat("#EXT-X-VERSION:7\n");
	playlist.AppendFormat("#EXT-X-INDEPENDENT-SEGMENTS\n");

	// Write EXT-X-MEDIA from _media_infos
	// shared lock
	std::shared_lock<std::shared_mutex> media_infos_lock(_media_groups_guard);
	for (const auto &[group_id, media_group] : _media_groups)
	{
		if (media_group->_used == false)
		{
			continue;
		}

		for (const auto &media_info : media_group->_media_infos)
		{
			playlist.AppendFormat("#EXT-X-MEDIA:TYPE=%s", media_info->GetTypeStr().CStr());
			playlist.AppendFormat(",GROUP-ID=\"%s\"", media_info->_group_id.CStr());
			playlist.AppendFormat(",NAME=\"%s\"", media_info->_name.CStr());
			playlist.AppendFormat(",DEFAULT=%s", media_info->_default ? "YES" : "NO");
			playlist.AppendFormat(",AUTOSELECT=%s", media_info->_auto_select ? "YES" : "NO");
			
			if (media_info->_type == cmn::MediaType::Audio)
			{
				playlist.AppendFormat(",CHANNELS=\"%d\"", media_info->_track->GetChannel().GetCounts() == 0 ? 2 : media_info->_track->GetChannel().GetCounts());
			}

			if (!media_info->_language.IsEmpty())
			{
				playlist.AppendFormat(",LANGUAGE=\"%s\"", media_info->_language.CStr());
			}

			if (!media_info->_instream_id.IsEmpty())
			{
				playlist.AppendFormat(",INSTREAM-ID=\"%s\"", media_info->_instream_id.CStr());
			}

			// _assoc_language
			if (!media_info->_assoc_language.IsEmpty())
			{
				playlist.AppendFormat(",ASSOCIATED-LANGUAGE=\"%s\"", media_info->_assoc_language.CStr());
			}

			if (!media_info->_uri.IsEmpty())
			{
				playlist.AppendFormat(",URI=\"%s%s", include_path?_chunk_path.CStr():"", media_info->_uri.CStr());

				if (chunk_query_string.IsEmpty() == false)
				{
					playlist.AppendFormat("?%s", chunk_query_string.CStr());
				}

				if (legacy)
				{
					if (chunk_query_string.IsEmpty() == false)
					{
						playlist.AppendFormat("&_HLS_legacy=YES");
					}
					else
					{
						playlist.AppendFormat("?_HLS_legacy=YES");
					}
				}
			}

			playlist.AppendFormat("\"\n");
		}
	}
	media_infos_lock.unlock();

	playlist.AppendFormat("\n");

	// Write EXT-X-STREAM-INF from _stream_infos
	// shared lock
	std::shared_lock<std::shared_mutex> stream_infos_lock(_stream_infos_guard);
	for (const auto &stream_info : _stream_infos)
	{
		playlist.AppendFormat("#EXT-X-STREAM-INF:");

		// Output Bandwidth
		playlist.AppendFormat("BANDWIDTH=%u", stream_info->_bandwidth == 0 ? 1000000 : stream_info->_bandwidth);

		// If variant_info is video, output RESOLUTION, FRAME-RATE
		if (stream_info->_media_info->_type == cmn::MediaType::Video)
		{
			playlist.AppendFormat(",RESOLUTION=%dx%d", stream_info->_width, stream_info->_height);
			// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.6.2
			// The value is a decimal-floating-point describing the maximum frame
      		// rate for all the video in the Variant Stream, rounded to three
      		// decimal places.
			playlist.AppendFormat(",FRAME-RATE=%.3f", stream_info->_framerate == 0 ? 30.0 : stream_info->_framerate);
		}

		// CODECS
		playlist.AppendFormat(",CODECS=\"%s\"", stream_info->_codecs.CStr());
		
		if (stream_info->_video_group_id.IsEmpty() == false)
		{
			playlist.AppendFormat(",VIDEO=\"%s\"", stream_info->_video_group_id.CStr());
		}

		if (stream_info->_audio_group_id.IsEmpty() == false)
		{
			playlist.AppendFormat(",AUDIO=\"%s\"", stream_info->_audio_group_id.CStr());
		}

		// URI
		playlist.AppendFormat("\n");
		playlist.AppendFormat("%s%s", include_path?_chunk_path.CStr():"", stream_info->_media_info->_uri.CStr());
		if (chunk_query_string.IsEmpty() == false)
		{
			playlist.AppendFormat("?%s", chunk_query_string.CStr());
		}
		if (legacy)
		{
			if (chunk_query_string.IsEmpty() == false)
			{
				playlist.AppendFormat("&_HLS_legacy=YES");
			}
			else
			{
				playlist.AppendFormat("?_HLS_legacy=YES");
			}
		}
		playlist.AppendFormat("\n");
	}
	stream_infos_lock.unlock();

	return playlist;
}

ov::String LLHlsMasterPlaylist::ToString(const ov::String &chunk_query_string, bool legacy, bool include_path) const
{
	if (chunk_query_string.IsEmpty() && legacy == false && include_path == true && !_cached_default_playlist.IsEmpty())
	{
		std::shared_lock<std::shared_mutex> lock(_cached_default_playlist_guard);
		return _cached_default_playlist;
	}

	return MakePlaylist(chunk_query_string, legacy, include_path);
}

std::shared_ptr<const ov::Data> LLHlsMasterPlaylist::ToGzipData(const ov::String &chunk_query_string, bool legacy) const
{
	if (chunk_query_string.IsEmpty() && legacy == false && _cached_default_playlist_gzip != nullptr)
	{
		std::shared_lock<std::shared_mutex> lock(_cached_default_playlist_gzip_guard);
		return _cached_default_playlist_gzip;
	}

	return  ov::Zip::CompressGzip(ToString(chunk_query_string, legacy).ToData(false));
}