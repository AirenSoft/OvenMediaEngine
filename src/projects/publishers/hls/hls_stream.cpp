//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_stream.h"

#include <base/ovlibrary/hex.h>
#include <config/config_manager.h>

#include <pugixml-1.9/src/pugixml.hpp>

#include <base/publisher/application.h>
#include <base/publisher/stream.h>

#include "hls_application.h"
#include "hls_session.h"
#include "hls_private.h"

/*

[Legacy HLS (Version 3) URLs]

* Master Playlist
http[s]://<host>:<port>/<application_name>/<stream_name>/ts:playlist.m3u8 
http[s]://<host>:<port>/<application_name>/<stream_name>/playlist.m3u8?format=ts

* Media Playlist
http[s]://<host>:<port>/<application_name>/<stream_name>/medialist_<variant_name>_hls.m3u8

* Segment File
http[s]://<host>:<port>/<application_name>/<stream_name>/seg_<variant_name>_<number>_hls.ts


[LL-HLS URLs] for reference

* Master Playlist
http[s]://<host>:<port>/[<application_name>/]<stream_name>/playlist.m3u8

* Media Playlist
http[s]://<host>:<port>/[<application_name>/]<stream_name>/chunklist_*_llhls.m3u8

* Segment File
http[s]://<host>:<port>/[<application_name>/]<stream_name>/init_*_llhls.m4s
http[s]://<host>:<port>/[<application_name>/]<stream_name>/part_*_llhls.m4s
http[s]://<host>:<port>/[<application_name>/]<stream_name>/seg_*_llhls.m4s

*/

std::shared_ptr<HlsStream> HlsStream::Create(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count)
{
	auto stream = std::make_shared<HlsStream>(application, info, worker_count);
	return stream;
}

HlsStream::HlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count)
	: Stream(application, info), _worker_count(worker_count)
{
}

HlsStream::~HlsStream()
{
	logtd("TsStream(%s/%s) has been terminated finally", GetApplicationName(), GetName().CStr());
}

bool HlsStream::Start()
{
	if (GetState() != State::CREATED)
	{
		return false;
	}

	if (CreateStreamWorker(_worker_count) == false)
	{
		return false;
	}

	auto config = GetApplication()->GetConfig();
	_ts_config = config.GetPublishers().GetHlsPublisher();

	_default_option_rewind = _ts_config.GetDefaultQueryString().GetBoolValue("_HLS_rewind", kDefaultHlsRewind);

	CreateDefaultPlaylist();

	// Create Packetizer
	if (CreatePackagers() == false)
	{
		logte("Failed to create packetizers");
		return false;
	}

	logti("HLS Stream has been created : %s/%u\nSegment Duration(%u) Segment Count(%u)", GetName().CStr(), GetId(),
		  _ts_config.GetSegmentDuration(), _ts_config.GetSegmentCount());

	return Stream::Start();
}

bool HlsStream::Stop()
{
	logtd("TsStream(%s) has been stopped", GetName().CStr());

	{
		std::lock_guard<std::shared_mutex> lock(_packetizers_guard);
		_packetizers.clear();
		_track_packetizers.clear();
	}

	{
		std::lock_guard<std::shared_mutex> lock(_media_playlists_guard);
		_media_playlists.clear();
	}

	{
		std::lock_guard<std::shared_mutex> lock(_packagers_guard);
		_packagers.clear();
	}

	{
		std::lock_guard<std::shared_mutex> lock(_master_playlists_guard);
		_master_playlists.clear();
	
	}

	return Stream::Stop();
}

bool HlsStream::IsSupportedCodec(cmn::MediaCodecId codec_id) const
{
	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::Aac:
			return true;
		default:
			return false;
	}

	return false;
}

bool HlsStream::CreateDefaultPlaylist()
{
	std::shared_ptr<MediaTrack> first_video_track = nullptr, first_audio_track = nullptr;
	for (const auto &[id, track] : _tracks)
	{
		if (IsSupportedCodec(track->GetCodecId()) == true)
		{
			// For default llhls.m3u8
			if (first_video_track == nullptr && track->GetMediaType() == cmn::MediaType::Video)
			{
				first_video_track = track;
			}
			else if (first_audio_track == nullptr && track->GetMediaType() == cmn::MediaType::Audio)
			{
				first_audio_track = track;
			}
		}
		else
		{
			logti("LLHlsStream(%s/%s) - Ignore unsupported codec(%s)", GetApplication()->GetName().CStr(), GetName().CStr(), StringFromMediaCodecId(track->GetCodecId()).CStr());
			continue;
		}
	}

	if (first_video_track == nullptr && first_audio_track == nullptr)
	{
		logtw("HLS stream [%s/%s] could not be created because there is no supported codec.", GetApplication()->GetName().CStr(), GetName().CStr());
		return false;
	}

	// Create default playlist
	ov::String default_playlist_name = TS_HLS_DEFAULT_PLAYLIST_NAME;
	auto default_playlist_name_without_ext = default_playlist_name.Substring(0, default_playlist_name.IndexOfRev('.'));
	auto default_playlist = Stream::GetPlaylist(default_playlist_name_without_ext);
	if (default_playlist == nullptr)
	{
		auto playlist = std::make_shared<info::Playlist>("hls_default", default_playlist_name_without_ext);
		auto rendition = std::make_shared<info::Rendition>("default", first_video_track ? first_video_track->GetVariantName() : "", first_audio_track ? first_audio_track->GetVariantName() : "");

		playlist->AddRendition(rendition);
		playlist->EnableTsPackaging(true);
		AddPlaylist(playlist);
	}

	return true;
}

void HlsStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	std::shared_lock<std::shared_mutex> lock(_packetizers_guard);
	auto& packetizers = _track_packetizers[media_packet->GetTrackId()];
	for (auto& packetizer : packetizers)
	{
		packetizer->AppendFrame(media_packet);
	}
}

void HlsStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) 
{
	std::shared_lock<std::shared_mutex> lock(_packetizers_guard);
	auto& packetizers = _track_packetizers[media_packet->GetTrackId()];
	for (auto& packetizer : packetizers)
	{
		packetizer->AppendFrame(media_packet);
	}
}

void HlsStream::OnSegmentCreated(const ov::String &packager_id, const std::shared_ptr<mpegts::Segment> &segment)
{
	auto playlist = GetMediaPlaylist(packager_id);
	if (playlist == nullptr)
	{
		logte("Failed to find the media playlist");
		return;
	}

	segment->SetUrl(GetSegmentName(packager_id, segment->GetNumber()));

	playlist->OnSegmentCreated(segment);

	logtd("Playlist : %s", playlist->ToString(false).CStr());
}

void HlsStream::OnSegmentDeleted(const ov::String &packager_id, const std::shared_ptr<mpegts::Segment> &segment)
{
	auto playlist = GetMediaPlaylist(packager_id);
	if (playlist == nullptr)
	{
		logte("Failed to find the media playlist");
		return;
	}

	playlist->OnSegmentDeleted(segment);
}

void HlsStream::SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	// Not implemented
	// TODO(getroot) : Implement this function
}

bool HlsStream::CreatePackagers()
{
	for (auto &it : GetPlaylists())
	{
		auto playlist = it.second;
		if (playlist == nullptr)
		{
			continue;
		}

		if (playlist->IsTsPackagingEnabled() == false)
		{
			continue;
		}

		HlsMasterPlaylist::Config master_playlist_config;

		auto master_playlist = std::make_shared<HlsMasterPlaylist>(playlist->GetFileName(), master_playlist_config);
		master_playlist->SetDefaultOption(_default_option_rewind);

		{
			std::lock_guard<std::shared_mutex> lock(_master_playlists_guard);
			_master_playlists.emplace(master_playlist->GetFileName(), master_playlist);
			logtd("HLS Master Playlist has been created : %s", master_playlist->GetFileName().CStr());
		}

		for (const auto &rendition : playlist->GetRenditionList())
		{
			// MediaPlaylist
			// Packager
			// Packetizer

			auto video_variant_name = rendition->GetVideoVariantName();
			auto audio_variant_name = rendition->GetAudioVariantName();
			auto variant_name = GetVariantName(video_variant_name, audio_variant_name);

			auto media_playlist = GetMediaPlaylist(variant_name);
			if (media_playlist != nullptr)
			{
				// Variant is already created, we can use the existing media playlist
				master_playlist->AddMediaPlaylist(media_playlist);
				continue;
			}

			// Already created
			if (GetPacketizer(variant_name) != nullptr)
			{
				continue;
			}

			//////////////////////////////////
			// Create Media Playlist
			//////////////////////////////////
			HlsMediaPlaylist::HlsMediaPlaylistConfig media_playlist_config;
			media_playlist_config.segment_count = _ts_config.GetSegmentCount();
			media_playlist_config.target_duration = _ts_config.GetSegmentDuration();
			media_playlist_config.event_playlist_type = _ts_config.GetDvr().IsEventPlaylistType();

			auto media_playlist_name = GetMediaPlaylistName(variant_name);
			media_playlist = std::make_shared<HlsMediaPlaylist>(variant_name, media_playlist_name, media_playlist_config);
			if (media_playlist == nullptr)
			{
				logte("Failed to create media playlist");
				return false;
			}

			{
				std::lock_guard<std::shared_mutex> lock(_media_playlists_guard);
				_media_playlists.emplace(variant_name, media_playlist);
			}

			//////////////////////////////////
			// Create Packager
			//////////////////////////////////
			mpegts::Packager::Config packager_config;
			packager_config.target_duration_ms = _ts_config.GetSegmentDuration() * 1000;
			packager_config.max_segment_count = _ts_config.GetSegmentCount();
			
			auto dvr_config = _ts_config.GetDvr();
			if (dvr_config.IsEnabled())
			{
				packager_config.dvr_window_ms = dvr_config.GetMaxDuration() * 1000;
				packager_config.dvr_storage_path = dvr_config.GetTempStoragePath();
			}

			auto packager = std::make_shared<mpegts::Packager>(variant_name, packager_config);
			if (packager == nullptr)
			{
				logte("Failed to create packager");
				return false;
			}

			{
				std::lock_guard<std::shared_mutex> lock(_packagers_guard);
				_packagers.emplace(variant_name, packager);
			}

			packager->AddSink(mpegts::PackagerSink::GetSharedPtr());

			//////////////////////////////////
			// Create Packetizer
			//////////////////////////////////
			auto packetizer = std::make_shared<mpegts::Packetizer>();
			if (packetizer == nullptr)
			{
				logte("Failed to create packager");
				return false;
			}

			{
				std::lock_guard<std::shared_mutex> lock(_packetizers_guard);
				_packetizers.emplace(variant_name, packetizer);
			}

			packetizer->AddSink(packager);

			if (video_variant_name.IsEmpty() == false)
			{
				auto video_track_group = GetMediaTrackGroup(video_variant_name);
				for (auto &track : video_track_group->GetTracks())
				{
					packetizer->AddTrack(track);

					// Add to track packetizers
					{
						std::lock_guard<std::shared_mutex> lock(_packetizers_guard);
						_track_packetizers[track->GetId()].emplace_back(packetizer);
					}

					media_playlist->AddMediaTrackInfo(track);

					// EXT-X-MEDIA is supported in EXT-X-VERSION 4 or higher
					// Now we support EXT-X-VERSION 3
					// Therefore, we don't support multiple video tracks now
					break;
				}
			}

			if (audio_variant_name.IsEmpty() == false)
			{
				auto audio_track_group = GetMediaTrackGroup(audio_variant_name);
				for (auto &track : audio_track_group->GetTracks())
				{
					packetizer->AddTrack(track);

					// Add to track packetizers
					{
						std::lock_guard<std::shared_mutex> lock(_packetizers_guard);
						_track_packetizers[track->GetId()].emplace_back(packetizer);
					}

					media_playlist->AddMediaTrackInfo(track);

					// EXT-X-MEDIA is supported in EXT-X-VERSION 4 or higher
					// Now we support EXT-X-VERSION 3
					// Therefore, we don't support multiple audio tracks now
					break;
				}
			}

			master_playlist->AddMediaPlaylist(media_playlist);
			packetizer->Start();
		}
	}

	return true;
}

ov::String HlsStream::GetVariantName(const ov::String &video_variant_name, const ov::String &audio_variant_name) const
{
	auto variant_name = ov::String::FormatString("%s_%s", video_variant_name.CStr(), audio_variant_name.CStr());
	return ov::Converter::ToString(variant_name.Hash());
}

std::shared_ptr<HlsMasterPlaylist> HlsStream::GetMasterPlaylist(const ov::String &playlist_name)
{
	std::shared_lock<std::shared_mutex> lock(_master_playlists_guard);

	auto it = _master_playlists.find(playlist_name);
	if (it == _master_playlists.end())
	{
		return nullptr;
	}

	return it->second;
}

ov::String HlsStream::GetMediaPlaylistName(const ov::String &variant_name) const
{
	return ov::String::FormatString("medialist_%s_hls.m3u8", variant_name.CStr());
}

ov::String HlsStream::GetSegmentName(const ov::String &variant_name, uint32_t number) const
{
	return ov::String::FormatString("seg_%s_%u_hls.ts", variant_name.CStr(), number);
}

std::shared_ptr<mpegts::Packetizer> HlsStream::GetPacketizer(const ov::String &variant_name)
{
	std::shared_lock<std::shared_mutex> lock(_packetizers_guard);

	auto it = _packetizers.find(variant_name);
	if (it == _packetizers.end())
	{
		return nullptr;
	}

	return it->second;
}

std::shared_ptr<mpegts::Packager> HlsStream::GetPackager(const ov::String &variant_name)
{
	std::shared_lock<std::shared_mutex> lock(_packagers_guard);

	auto it = _packagers.find(variant_name);
	if (it == _packagers.end())
	{
		return nullptr;
	}

	return it->second;
}

std::shared_ptr<HlsMediaPlaylist> HlsStream::GetMediaPlaylist(const ov::String &variant_name)
{
	std::shared_lock<std::shared_mutex> lock(_media_playlists_guard);

	auto it = _media_playlists.find(variant_name);
	if (it == _media_playlists.end())
	{
		return nullptr;
	}

	return it->second;
}

std::tuple<HlsStream::RequestResult, std::shared_ptr<const ov::Data>> HlsStream::GetMasterPlaylistData(const ov::String &playlist_name, bool rewind)
{
	auto playlist = GetMasterPlaylist(playlist_name);
	if (playlist == nullptr)
	{
		return {RequestResult::NotFound, nullptr};
	}

	auto data = playlist->ToString(rewind).ToData(false);
	if (data == nullptr)
	{
		return std::make_tuple(RequestResult::UnknownError, nullptr);
	}

	return std::make_tuple(RequestResult::Success, data);
}

std::tuple<HlsStream::RequestResult, std::shared_ptr<const ov::Data>> HlsStream::GetMediaPlaylistData(const ov::String &variant_name, bool rewind)
{
	auto playlist = GetMediaPlaylist(variant_name);
	if (playlist == nullptr)
	{
		return std::make_tuple(RequestResult::NotFound, nullptr);
	}

	auto data = playlist->ToString(rewind).ToData(false);
	if (data == nullptr)
	{
		return std::make_tuple(RequestResult::UnknownError, nullptr);
	}

	return std::make_tuple(RequestResult::Success, data);
}

std::tuple<HlsStream::RequestResult, std::shared_ptr<const ov::Data>> HlsStream::GetSegmentData(const ov::String &variant_name, uint32_t number)
{
	auto packager = GetPackager(variant_name);
	if (packager == nullptr)
	{
		return std::make_tuple(RequestResult::NotFound, nullptr);
	}

	auto segment_data = packager->GetSegmentData(number);
	if (segment_data == nullptr)
	{
		return std::make_tuple(RequestResult::NotFound, nullptr);
	}

	return std::make_tuple(RequestResult::Success, segment_data);
}