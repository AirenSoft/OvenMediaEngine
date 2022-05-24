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

bool LLHlsMasterPlaylist::AddGroupMedia(const MediaInfo &media_info)
{
	if (media_info._group_id.IsEmpty())
	{
		return false;
	}

	// Validate media info
	// TODO(Getroot) : It it better to validate media info in the Config Module
	auto it = _media_infos.find(media_info._group_id);
	if (it != _media_infos.end())
	{
		auto first = it->second.front();
		if (first._type != media_info._type || 
			first._track->GetCodecId() != media_info._track->GetCodecId())
		{
			return false;
		}
	}

	// lock
	std::lock_guard<std::shared_mutex> lock(_media_infos_guard);
	_media_infos[media_info._group_id].push_back(media_info);

	return true;
}

// Video + Audio + Subtitle + ClosedCaptions
// But now OME support only Video + Audio
bool LLHlsMasterPlaylist::AddStreamInfo(const StreamInfo &stream_info)
{
	if (stream_info._uri.IsEmpty())
	{
		return false;
	}

	// Video group
	if (!stream_info._video_group_id.IsEmpty())
	{
		SetActiveMediaGroup(stream_info._video_group_id);
	}

	// Audio group
	if (!stream_info._audio_group_id.IsEmpty())
	{
		SetActiveMediaGroup(stream_info._audio_group_id);
	}

	// Subtitle group
	if (!stream_info._subtitle_group_id.IsEmpty())
	{
		SetActiveMediaGroup(stream_info._subtitle_group_id);
	}

	// Closed captions group
	if (!stream_info._closed_captions_group_id.IsEmpty())
	{
		SetActiveMediaGroup(stream_info._closed_captions_group_id);
	}
	
	// lock
	std::lock_guard<std::shared_mutex> lock(_stream_infos_guard);
	_stream_infos.push_back(stream_info);

	return true;
}

bool LLHlsMasterPlaylist::SetActiveMediaGroup(const ov::String &group_id)
{
	if (group_id.IsEmpty() == false)
	{
		std::shared_lock<std::shared_mutex> media_infos_lock(_media_infos_guard);
		auto it = _media_infos.find(group_id);
		if (it == _media_infos.end())
		{
			return false;
		}

		auto &media_infos = it->second;
		for (auto &media_info : media_infos)
		{
			media_info._used = true;
		}
	}

	return true;
}

const LLHlsMasterPlaylist::MediaInfo &LLHlsMasterPlaylist::GetDefaultMediaInfo(const ov::String &group_id) const
{
	std::shared_lock<std::shared_mutex> media_infos_lock(_media_infos_guard);

	auto it = _media_infos.find(group_id);
	if (it == _media_infos.end())
	{
		return MediaInfo::InvalidMediaInfo();
	}

	auto &media_infos = it->second;
	for (auto &media_info : media_infos)
	{
		if (media_info._default == true)
		{
			return media_info;
		}
	}

	return MediaInfo::InvalidMediaInfo();
}

ov::String LLHlsMasterPlaylist::ToString(const ov::String &chunk_query_string) const
{
	ov::String playlist(1024);

	playlist.AppendFormat("#EXTM3U\n");
	playlist.AppendFormat("#EXT-X-VERSION:7\n");
	playlist.AppendFormat("#EXT-X-INDEPENDENT-SEGMENTS\n");

	// Write EXT-X-MEDIA from _media_infos
	// shared lock
	std::shared_lock<std::shared_mutex> media_infos_lock(_media_infos_guard);
	for (const auto &[group_id, media_infos] : _media_infos)
	{
		for (const auto &media_info : media_infos)
		{
			if (media_info._used)
			{
				playlist.AppendFormat("#EXT-X-MEDIA:TYPE=%s", media_info.TypeString().CStr());
				playlist.AppendFormat(",GROUP-ID=\"%s\"", media_info._group_id.CStr());
				playlist.AppendFormat(",NAME=\"%s\"", media_info._name.CStr());
				playlist.AppendFormat(",AUTOSELECT=%s", media_info._auto_select ? "YES" : "NO");
				playlist.AppendFormat(",DEFAULT=%s", media_info._default ? "YES" : "NO");

				if (media_info._type == MediaInfo::Type::Audio)
				{
					playlist.AppendFormat(",CHANNELS=\"%d\"", media_info._track->GetChannel().GetCounts() == 0 ? 2 : media_info._track->GetChannel().GetCounts());
				}

				if (!media_info._language.IsEmpty())
				{
					playlist.AppendFormat(",LANGUAGE=\"%s\"", media_info._language.CStr());
				}

				if (!media_info._instream_id.IsEmpty())
				{
					playlist.AppendFormat(",INSTREAM-ID=\"%s\"", media_info._instream_id.CStr());
				}

				// _assoc_language
				if (!media_info._assoc_language.IsEmpty())
				{
					playlist.AppendFormat(",ASSOCIATED-LANGUAGE=\"%s\"", media_info._assoc_language.CStr());
				}

				if (!media_info._uri.IsEmpty())
				{
					playlist.AppendFormat(",URI=\"%s", media_info._uri.CStr());

					if (chunk_query_string.IsEmpty() == false)
					{
						playlist.AppendFormat("?%s", chunk_query_string.CStr());
					}
				}

				playlist.AppendFormat("\"\n");
			}
		}
	}
	media_infos_lock.unlock();

	playlist.AppendFormat("\n");

	// Write EXT-X-STREAM-INF from _stream_infos
	// shared lock
	std::shared_lock<std::shared_mutex> stream_infos_lock(_stream_infos_guard);
	for (const auto &variant_info : _stream_infos)
	{
		playlist.AppendFormat("#EXT-X-STREAM-INF:");

		std::shared_ptr<const MediaTrack> video_track = nullptr, audio_track = nullptr;

		if (variant_info._track->GetMediaType() == cmn::MediaType::Video)
		{
			video_track = variant_info._track;
			if (variant_info._audio_group_id.IsEmpty() == false)
			{
				auto &audio_media_info = GetDefaultMediaInfo(variant_info._audio_group_id);
				audio_track = audio_media_info._track;
			}
		}
		else if (variant_info._track->GetMediaType() == cmn::MediaType::Audio)
		{
			audio_track = variant_info._track;
			if (variant_info._video_group_id.IsEmpty() == false)
			{
				auto &video_media_info = GetDefaultMediaInfo(variant_info._video_group_id);
				video_track = video_media_info._track;
			}
		}

		// BANDWIDTH
		uint32_t bandwidth = 0;

		if (video_track != nullptr)
		{
			bandwidth += video_track->GetBitrate();
		}
		
		if (audio_track != nullptr)
		{
			bandwidth += audio_track->GetBitrate();
		}

		// Output Bandwidth
		playlist.AppendFormat("BANDWIDTH=%d", bandwidth == 0 ? 2000000 : bandwidth);

		// If variant_info is video, output RESOLUTION, FRAME-RATE
		if (variant_info._track->GetMediaType() == cmn::MediaType::Video)
		{
			playlist.AppendFormat(",RESOLUTION=%dx%d", variant_info._track->GetWidth(), variant_info._track->GetHeight());
			playlist.AppendFormat(",FRAME-RATE=%.1f", variant_info._track->GetFrameRate() == 0 ? 30.0 : variant_info._track->GetFrameRate());
		}

		// CODECS
		playlist.AppendFormat(",CODECS=\"");

		if (video_track != nullptr)
		{
			playlist.AppendFormat("%s", CodecMediaType::GetCodecsParameter(video_track).CStr());
		}
		if (audio_track != nullptr)
		{
			if (video_track != nullptr)
			{
				playlist.AppendFormat(",");
			}
			playlist.AppendFormat("%s", CodecMediaType::GetCodecsParameter(audio_track).CStr());
		}
		playlist.AppendFormat("\"");

		if (variant_info._video_group_id.IsEmpty() == false)
		{
			playlist.AppendFormat(",VIDEO=\"%s\"", variant_info._video_group_id.CStr());
		}

		if (variant_info._audio_group_id.IsEmpty() == false)
		{
			playlist.AppendFormat(",AUDIO=\"%s\"", variant_info._audio_group_id.CStr());
		}

		// URI
		playlist.AppendFormat("\n");
		playlist.AppendFormat("%s", variant_info._uri.CStr());
		if (chunk_query_string.IsEmpty() == false)
		{
			playlist.AppendFormat("?%s", chunk_query_string.CStr());
		}
		playlist.AppendFormat("\n");
	}
	stream_infos_lock.unlock();

	return playlist;
}

std::shared_ptr<const ov::Data> LLHlsMasterPlaylist::ToGzipData(const ov::String &chunk_query_string) const
{
	return  ov::Zip::CompressGzip(ToString(chunk_query_string).ToData(false));
}