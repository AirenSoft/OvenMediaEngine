//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "llhls_stream.h"

#include <base/ovlibrary/hex.h>
#include <config/config_manager.h>

#include <pugixml-1.9/src/pugixml.hpp>

#include <base/publisher/application.h>
#include <base/publisher/stream.h>

#include "llhls_application.h"
#include "llhls_session.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsStream> LLHlsStream::Create(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count)
{
	auto stream = std::make_shared<LLHlsStream>(application, info, worker_count);
	return stream;
}

LLHlsStream::LLHlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count)
	: Stream(application, info), _worker_count(worker_count)
{
}

LLHlsStream::~LLHlsStream()
{
	logtd("LLHlsStream(%s/%s) has been terminated finally", GetApplicationName(), GetName().CStr());
}

bool LLHlsStream::Start()
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

	auto llhls_config = config.GetPublishers().GetLLHlsPublisher();
	auto dump_config = llhls_config.GetDumps();
	auto dvr_config = llhls_config.GetDvr();

	if (llhls_config.IsOriginMode())
	{
		_stream_key = ov::String::FormatString("%zu", GetName().Hash());
	}
	else
	{
		_stream_key = ov::Random::GenerateString(8);
	}

	auto drm_config = llhls_config.GetDrm();
	if (drm_config.IsEnabled())
	{
		GetDrmInfo(drm_config.GetDrmInfoPath(), _cenc_property);
	}

	_packager_config.chunk_duration_ms = llhls_config.GetChunkDuration() * 1000.0;
	_packager_config.segment_duration_ms = llhls_config.GetSegmentDuration() * 1000.0;
	// cenc property will be set in AddPackager

	_storage_config.max_segments = llhls_config.GetSegmentCount();
	_storage_config.segment_duration_ms = llhls_config.GetSegmentDuration() * 1000;
	_storage_config.dvr_enabled = dvr_config.IsEnabled();
	_storage_config.dvr_storage_path = dvr_config.GetTempStoragePath();
	_storage_config.dvr_duration_sec = dvr_config.GetMaxDuration();
	_storage_config.server_time_based_segment_numbering = llhls_config.IsServerTimeBasedSegmentNumbering();

	_configured_part_hold_back = llhls_config.GetPartHoldBack();
	_preload_hint_enabled = llhls_config.IsPreloadHintEnabled();

	// Find data track
	auto data_track = GetFirstTrackByType(cmn::MediaType::Data);

	std::shared_ptr<MediaTrack> first_video_track = nullptr, first_audio_track = nullptr;
	for (const auto &[id, track] : _tracks)
	{
		if (IsSupportedCodec(track->GetCodecId()) == true)
		{
			if (AddPackager(track, data_track) == false)
			{
				logte("LLHlsStream(%s/%s) - Failed to add packager for track(%ld)", GetApplication()->GetName().CStr(), GetName().CStr(), track->GetId());
				return false;
			}

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
			if (track == data_track)
			{
				continue;
			}

			logti("LLHlsStream(%s/%s) - Ignore unsupported codec(%s)", GetApplication()->GetName().CStr(), GetName().CStr(), StringFromMediaCodecId(track->GetCodecId()).CStr());
			continue;
		}
	}

	// Set renditions to each chunklist writer
	{
		std::lock_guard<std::shared_mutex> lock(_chunklist_map_lock);
		for (auto &it : _chunklist_map)
		{
			auto chunklist_writer = it.second;
			chunklist_writer->SetRenditions(_chunklist_map);
		}
	}

	if (first_video_track == nullptr && first_audio_track == nullptr)
	{
		logtw("LLHLS stream [%s/%s] could not be created because there is no supported codec.", GetApplication()->GetName().CStr(), GetName().CStr());
		return false;
	}

	// If there is no default playlist, make default playlist
	// Default playlist is consist of first compatible video and audio track among all tracks
	ov::String default_playlist_name = DEFAULT_PLAYLIST_NAME;
	auto default_playlist_name_without_ext = default_playlist_name.Substring(0, default_playlist_name.IndexOfRev('.'));
	auto default_playlist = Stream::GetPlaylist(default_playlist_name_without_ext);
	if (default_playlist == nullptr)
	{
		auto playlist = std::make_shared<info::Playlist>("llhls_default", default_playlist_name_without_ext);
		auto rendition = std::make_shared<info::Rendition>("default", first_video_track ? first_video_track->GetVariantName() : "", first_audio_track ? first_audio_track->GetVariantName() : "");

		playlist->AddRendition(rendition);

		AddPlaylist(playlist);

		auto master_playlist = CreateMasterPlaylist(playlist);

		std::lock_guard<std::mutex> guard(_master_playlists_lock);
		_master_playlists[default_playlist_name] = master_playlist;
	}

	// Select the dump setting for this stream.
	std::lock_guard<std::shared_mutex> lock(_dumps_lock);
	for (auto dump : dump_config.GetDumps())
	{
		if (dump.IsEnabled() == false)
		{
			continue;
		}

		// check if dump.TargetStreamName is same as this stream name
		auto match_result = dump.GetTargetStreamNameRegex().Matches(GetName().CStr());
		if (match_result.IsMatched())
		{
			// Replace output path with macro
			auto output_path = dump.GetOutputPath();
			// ${VHostName}, ${AppName}, ${StreamName}
			output_path = output_path.Replace("${VHostName}", GetApplication()->GetName().GetVHostName().CStr());
			output_path = output_path.Replace("${AppName}", GetApplication()->GetName().GetAppName().CStr());
			output_path = output_path.Replace("${StreamName}", GetName().CStr());

			// ${YYYY}, ${MM}, ${DD}, ${hh}, ${mm}, ${ss}
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			struct tm tm;
			::localtime_r(&time, &tm);

			char tmbuf[2048];

			// Replace ${ISO8601} to ISO8601 format
			memset(tmbuf, 0, sizeof(tmbuf));
			::strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%dT%H:%M:%S%z", &tm);
			output_path = output_path.Replace("${ISO8601}", tmbuf);

			// Replace ${YYYY}, ${MM}, ${DD}, ${hh}, ${mm}, ${ss}
			output_path = output_path.Replace("${YYYY}", ov::Converter::ToString(tm.tm_year + 1900).CStr());
			output_path = output_path.Replace("${MM}", ov::String::FormatString("%02d", tm.tm_mon + 1).CStr());
			output_path = output_path.Replace("${DD}", ov::String::FormatString("%02d", tm.tm_mday).CStr());
			output_path = output_path.Replace("${hh}", ov::String::FormatString("%02d", tm.tm_hour).CStr());
			output_path = output_path.Replace("${mm}", ov::String::FormatString("%02d", tm.tm_min).CStr());
			output_path = output_path.Replace("${ss}", ov::String::FormatString("%02d", tm.tm_sec).CStr());
			output_path = output_path.Replace("${S}", tm.tm_zone);

			// +hhmm or -hhmm
			memset(tmbuf, 0, sizeof(tmbuf));
			strftime(tmbuf, sizeof(tmbuf), "%z", &tm);
			output_path = output_path.Replace("${z}", tmbuf);

			auto dump_item = std::make_shared<mdl::Dump>();
			dump_item->SetId(dump.GetId());
			dump_item->SetOutputPath(output_path);
			dump_item->SetPlaylists(dump.GetPlaylists());
			dump_item->SetEnabled(true);

			_dumps.emplace(dump_item->GetId(), dump_item);
		}
	}

	logti("LLHlsStream has been created : %s/%u\nOriginMode(%s) Chunk Duration(%.2f) Segment Duration(%.2f) Segment Count(%u) DRM(%s)", GetName().CStr(), GetId(),
		  ov::Converter::ToString(llhls_config.IsOriginMode()).CStr(), llhls_config.GetChunkDuration(), llhls_config.GetSegmentDuration(), llhls_config.GetSegmentCount(), bmff::CencProtectSchemeToString(_cenc_property.scheme));

	return Stream::Start();
}

bool LLHlsStream::Stop()
{
	logtd("LLHlsStream(%s) has been stopped", GetName().CStr());

	{
		std::scoped_lock lock{_packager_map_lock, _storage_map_lock, _chunklist_map_lock, _master_playlists_lock, _dumps_lock};

		// clear all packagers
		_packager_map.clear();

		// clear all storages
		_storage_map.clear();

		// clear all playlist
		for (auto &it : _chunklist_map)
		{
			auto chunklist_writer = it.second;
			chunklist_writer->Release();
		}

		_chunklist_map.clear();

		// complete all dumps
		for (auto &it : _dumps)
		{
			auto dump = it.second;
			dump->SetEnabled(false);
			dump->CompleteDump();
		}
	}

	return Stream::Stop();
}

bool LLHlsStream::GetDrmInfo(const ov::String &file_path, bmff::CencProperty &cenc_property)
{
	ov::String final_path = ov::GetFilePath(file_path, cfg::ConfigManager::GetInstance()->GetConfigPath());

	pugi::xml_document xml_doc;
	auto load_result = xml_doc.load_file(final_path.CStr());
	if (load_result == false)
	{
		logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) status(%d) description(%s)", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), load_result.status, load_result.description());
		return false;
	}

	auto root_node = xml_doc.child("DRMInfo");
	if (root_node.empty())
	{
		logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because root node is not found", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr());
		return false;
	}

	bool has_fairplay_pssh_box = false;

	for (pugi::xml_node drm_node = root_node.child("DRM"); drm_node; drm_node = drm_node.next_sibling("DRM"))
	{
		ov::String name = drm_node.child_value("Name");
		ov::String virtual_host_name = drm_node.child_value("VirtualHostName");
		ov::String app_name = drm_node.child_value("ApplicationName");
		ov::String stream_name = drm_node.child_value("StreamName");

		// stream_name can be regex
		ov::Regex _target_stream_name_regex = ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(stream_name));
		auto match_result = _target_stream_name_regex.Matches(GetName().CStr());

		if (virtual_host_name == GetApplication()->GetName().GetVHostName() &&
			app_name == GetApplication()->GetName().GetAppName() &&
			match_result.IsMatched())
		{
			ov::String drm_provider = drm_node.child_value("DRMProvider");

			if (drm_provider.IsEmpty() || drm_provider.LowerCaseString() == "manual")
			{
				ov::String cenc_protect_scheme = drm_node.child_value("CencProtectScheme");
				ov::String key_id = drm_node.child_value("KeyId");
				ov::String key = drm_node.child_value("Key");
				ov::String iv = drm_node.child_value("Iv");
				ov::String fairplay_key_url = drm_node.child_value("FairPlayKeyUrl");
				ov::String keyformat = drm_node.child_value("Keyformat");

				// required
				if (cenc_protect_scheme.IsEmpty() || key_id.IsEmpty() || key.IsEmpty() || iv.IsEmpty())
				{
					logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because required field is empty", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr());
					return false;
				}

				if (cenc_protect_scheme == "cbcs")
				{
					cenc_property.scheme = bmff::CencProtectScheme::Cbcs;
				}
				else if (cenc_protect_scheme == "cenc")
				{
					cenc_property.scheme = bmff::CencProtectScheme::Cenc;
				}
				else
				{
					logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because CencProtectScheme(%s) is not supported", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), cenc_protect_scheme.CStr());
				}

				cenc_property.key_id = ov::Hex::Decode(key_id);
				if (cenc_property.key_id == nullptr || cenc_property.key_id->GetLength() != 16)
				{
					logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because KeyId(%s) is invalid (must be 16 bytes HEX foramt)", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), key_id.CStr());
					cenc_property.scheme = bmff::CencProtectScheme::None;
					return false;
				}

				cenc_property.key = ov::Hex::Decode(key);
				if (cenc_property.key == nullptr || cenc_property.key->GetLength() != 16)
				{
					logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because Key(%s) is invalid (must be 16 bytes HEX format)", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), key.CStr());
					cenc_property.scheme = bmff::CencProtectScheme::None;
					return false;
				}

				cenc_property.iv = ov::Hex::Decode(iv);
				if (cenc_property.iv == nullptr || cenc_property.iv->GetLength() != 16)
				{
					logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because Iv(%s) is invalid (must be 16 bytes HEX format)", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), iv.CStr());
					cenc_property.scheme = bmff::CencProtectScheme::None;
					return false;
				}

				pugi::xml_node pssh_node = drm_node.child("Pssh");
				while (pssh_node)
				{
					auto pssh_box_data = ov::Hex::Decode(pssh_node.child_value());
					if (pssh_box_data == nullptr)
					{
						logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because Pssh(%s) is invalid (must be HEX format)", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), pssh_node.child_value());
						cenc_property.scheme = bmff::CencProtectScheme::None;
						return false;
					}

					auto pssh_box = bmff::PsshBox(pssh_box_data);
					cenc_property.pssh_box_list.push_back(pssh_box);

					if (pssh_box.drm_system == bmff::DRMSystem::FairPlay)
					{
						has_fairplay_pssh_box = true;
					}

					pssh_node = pssh_node.next_sibling("Pssh");
				}

				cenc_property.fairplay_key_uri = fairplay_key_url;
				cenc_property.keyformat = keyformat;
			}
			else
			{
				logte("LLHlsStream(%s/%s) - Failed to load DRM info file(%s) because DRMProvider(%s) is not supported", GetApplication()->GetName().CStr(), GetName().CStr(), final_path.CStr(), drm_provider.CStr());
				return false;
			}

			// Just first DRM info matched is enough for one stream
			break;
		}
	}

	// If cenc_property has fairplay_key_uri, but there is no pssh box for fairplay, add it.
	// default pssh box 
	if (cenc_property.fairplay_key_uri.IsEmpty() == false && has_fairplay_pssh_box == false)
	{
    	cenc_property.pssh_box_list.push_back(bmff::PsshBox("94ce86fb-07ff-4f43-adb8-93d2fa968ca2", {cenc_property.key_id}, nullptr));
	}

	// Set profiles
	if (cenc_property.scheme == bmff::CencProtectScheme::Cenc)
	{
		cenc_property.crypt_bytes_block = 0;
		cenc_property.skip_bytes_block = 0;
		cenc_property.per_sample_iv_size = 16;
	}
	else if (cenc_property.scheme == bmff::CencProtectScheme::Cbcs)
	{
		cenc_property.crypt_bytes_block = 1;
		cenc_property.skip_bytes_block = 9;
		cenc_property.per_sample_iv_size = 0;
	}

	return true;
}

bool LLHlsStream::IsSupportedCodec(cmn::MediaCodecId codec_id) const
{
	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::H265:
		case cmn::MediaCodecId::Aac:
			return true;
		default:
			return false;
	}

	return false;
}

const ov::String &LLHlsStream::GetStreamKey() const
{
	return _stream_key;
}

uint64_t LLHlsStream::GetMaxChunkDurationMS() const
{
	return _max_chunk_duration_ms;
}

std::shared_ptr<LLHlsMasterPlaylist> LLHlsStream::CreateMasterPlaylist(const std::shared_ptr<const info::Playlist> &playlist) const
{
	auto master_playlist = std::make_shared<LLHlsMasterPlaylist>();

	ov::String chunk_path;
	ov::String app_name = GetApplicationInfo().GetName().GetAppName();
	ov::String stream_name = GetName();
	switch (playlist->GetHlsChunklistPathDepth())
	{
		case 0:
			chunk_path = "";
			break;
		case 1:
			chunk_path = ov::String::FormatString("../%s/", stream_name.CStr());
			break;
		case 2:
			chunk_path = ov::String::FormatString("../../%s/%s/", app_name.CStr(), stream_name.CStr());
			break;
		case -1:
		default:
			chunk_path = ov::String::FormatString("/%s/%s/", app_name.CStr(), stream_name.CStr());
			break;
	}

	// default options
	auto llhls_conf = GetApplication()->GetConfig().GetPublishers().GetLLHlsPublisher();
	auto default_query_value_hls_legacy = llhls_conf.GetDefaultQueryString().GetBoolValue("_HLS_legacy", kDefaultHlsLegacy);
	auto default_query_value_hls_rewind = llhls_conf.GetDefaultQueryString().GetBoolValue("_HLS_rewind", kDefaultHlsRewind);

	master_playlist->SetDefaultOptions(default_query_value_hls_legacy, default_query_value_hls_rewind);
	master_playlist->SetChunkPath(chunk_path);
	master_playlist->SetCencProperty(_cenc_property);

	// Add all media candidates to master playlist
	for (const auto &[track_id, track_group] : GetMediaTrackGroups())
	{
		auto track = track_group->GetFirstTrack();
		if (track == nullptr)
		{
			continue;
		}

		if (IsSupportedCodec(track->GetCodecId()) == false)
		{
			continue;
		}

		master_playlist->AddMediaCandidateGroup(track_group, [=](const std::shared_ptr<const MediaTrack> &track) -> ov::String {
			return GetChunklistName(track->GetId());
		});
	}

	// Add stream
	for (const auto &rendition : playlist->GetRenditionList())
	{
		auto video_track = GetFirstTrackByVariant(rendition->GetVideoVariantName());
		auto audio_track = GetFirstTrackByVariant(rendition->GetAudioVariantName());

		if ((video_track != nullptr && IsSupportedCodec(video_track->GetCodecId()) == false) ||
			(audio_track != nullptr && IsSupportedCodec(audio_track->GetCodecId()) == false))
		{
			logtw("LLHlsStream(%s/%s) - Exclude the rendition(%s) from the %s.m3u8 due to unsupported codec", GetApplication()->GetName().CStr(), GetName().CStr(),
				  rendition->GetName().CStr(), playlist->GetFileName().CStr());
			continue;
		}

		// If there is no media track, it is not added to the master playlist
		ov::String video_variant_name = video_track != nullptr ? video_track->GetVariantName() : "";
		ov::String audio_variant_name = audio_track != nullptr ? audio_track->GetVariantName() : "";

		if (rendition->GetVideoVariantName().IsEmpty() == false && video_track == nullptr)
		{
			logtw("LLHlsStream(%s/%s) - %s video is excluded from the %s rendition in %s playlist because there is no video track.", GetApplication()->GetName().CStr(), GetName().CStr(), rendition->GetVideoVariantName().CStr(), rendition->GetName().CStr(), playlist->GetFileName().CStr());
		}

		if (rendition->GetAudioVariantName().IsEmpty() == false && audio_track == nullptr)
		{
			logtw("LLHlsStream(%s/%s) - %s audio is excluded from the %s rendition in %s playlist because there is no audio track.", GetApplication()->GetName().CStr(), GetName().CStr(), rendition->GetAudioVariantName().CStr(), rendition->GetName().CStr(), playlist->GetFileName().CStr());
		}

		master_playlist->AddStreamInfo(video_variant_name, audio_variant_name);
	}

	master_playlist->UpdateCacheForDefaultPlaylist();

	return master_playlist;
}

void LLHlsStream::DumpMasterPlaylistsOfAllItems()
{
	// lock
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);
	for (auto &it : _dumps)
	{
		auto dump = it.second;
		if (dump->IsEnabled() == false)
		{
			continue;
		}

		if (DumpMasterPlaylist(dump) == false)
		{
			// Event if the dump fails, it will not be deleted
			//dump->SetEnabled(false);
		}
	}
}

bool LLHlsStream::DumpMasterPlaylist(const std::shared_ptr<mdl::Dump> &item)
{
	if (item->IsEnabled() == false)
	{
		return false;
	}

	for (auto &playlist : item->GetPlaylists())
	{
		auto [result, data] = GetMasterPlaylist(playlist, "", false, false, true, false);
		if (result != RequestResult::Success)
		{
			logtw("Could not get master playlist(%s) for dump", playlist.CStr());
			return false;
		}

		if (DumpData(item, playlist, data) == false)
		{
			logtw("Could not dump master playlist(%s)", playlist.CStr());
			return false;
		}
	}

	return true;
}

void LLHlsStream::DumpInitSegmentOfAllItems(const int32_t &track_id)
{
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);
	for (auto &it : _dumps)
	{
		auto dump = it.second;
		if (dump->IsEnabled() == false)
		{
			continue;
		}

		if (DumpInitSegment(dump, track_id) == false)
		{
			dump->SetEnabled(false);
		}
	}
}

bool LLHlsStream::DumpInitSegment(const std::shared_ptr<mdl::Dump> &item, const int32_t &track_id)
{
	if (item->IsEnabled() == false)
	{
		logtw("Dump(%s) is disabled", item->GetId().CStr());
		return false;
	}

	// Get segment
	auto [result, data] = GetInitializationSegment(track_id);
	if (result != RequestResult::Success)
	{
		logtw("Could not get init segment for dump");
		return false;
	}

	auto init_segment_name = GetInitializationSegmentName(track_id);

	return DumpData(item, init_segment_name, data);
}

void LLHlsStream::DumpSegmentOfAllItems(const int32_t &track_id, const uint32_t &segment_number)
{
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);
	for (auto &it : _dumps)
	{
		auto dump = it.second;
		if (dump->IsEnabled() == false)
		{
			continue;
		}

		if (DumpSegment(dump, track_id, segment_number) == false)
		{
			dump->SetEnabled(false);
			continue;
		}
	}
}

bool LLHlsStream::DumpSegment(const std::shared_ptr<mdl::Dump> &item, const int32_t &track_id, const int64_t &segment_number)
{
	if (item->IsEnabled() == false)
	{
		return false;
	}

	if (item->HasExtraData(track_id) == false)
	{
		item->SetExtraData(track_id, segment_number);
	}

	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track(%s/%d)", GetName().CStr(), track_id);
		return false;
	}

	// Get segment
	auto segment = storage->GetMediaSegment(segment_number);
	if (segment == nullptr)
	{
		logtw("Could not get segment(%u) for dump", segment_number);
		return false;
	}

	auto segment_data = segment->GetData();

	// Get updated chunklist
	auto chunklist = GetChunklistWriter(track_id);
	if (chunklist == nullptr)
	{
		logtw("Could not find chunklist for track_id = %d", track_id);
		return false;
	}

	auto chunklist_data = chunklist->ToString("", false, true, false, true, item->GetFirstSegmentNumber(track_id)).ToData(false);

	auto segment_file_name = GetSegmentName(track_id, segment_number);
	auto chunklist_file_name = GetChunklistName(track_id);

	if (DumpData(item, segment_file_name, segment_data) == false)
	{
		logtw("Could not dump segment(%s)", segment_file_name.CStr());
		return false;
	}

	if (DumpData(item, chunklist_file_name, chunklist_data) == false)
	{
		logtw("Could not dump chunklist(%s)", chunklist_file_name.CStr());
		return false;
	}

	chunklist->SaveOldSegmentInfo(true);

	return true;
}

bool LLHlsStream::DumpData(const std::shared_ptr<mdl::Dump> &item, const ov::String &file_name, const std::shared_ptr<const ov::Data> &data)
{
	return item->DumpData(file_name, data);
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<const ov::Data>> LLHlsStream::GetMasterPlaylist(const ov::String &file_name, const ov::String &chunk_query_string, bool gzip, bool legacy, bool rewind, bool include_path)
{
	if (GetState() != State::STARTED)
	{
		return {RequestResult::NotFound, nullptr};
	}

	if (IsReadyToPlay() == false)
	{
		return {RequestResult::Accepted, nullptr};
	}

	std::shared_ptr<LLHlsMasterPlaylist> master_playlist = nullptr;

	// _master_playlists_lock
	std::unique_lock<std::mutex> guard(_master_playlists_lock);
	auto it = _master_playlists.find(file_name);
	if (it == _master_playlists.end())
	{
		auto file_name_without_ext = file_name.Substring(0, file_name.IndexOfRev('.'));

		// Create master playlist
		auto playlist = GetPlaylist(file_name_without_ext);
		if (playlist == nullptr)
		{
			return {RequestResult::NotFound, nullptr};
		}

		master_playlist = CreateMasterPlaylist(playlist);

		// Cache
		_master_playlists[file_name] = master_playlist;
	}
	else
	{
		master_playlist = it->second;
	}
	guard.unlock();

	if (master_playlist == nullptr)
	{
		return {RequestResult::NotFound, nullptr};
	}

	if (gzip == true)
	{
		return {RequestResult::Success, master_playlist->ToGzipData(chunk_query_string, legacy, rewind)};
	}

	return {RequestResult::Success, master_playlist->ToString(chunk_query_string, legacy, rewind, include_path).ToData(false)};
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<const ov::Data>> LLHlsStream::GetChunklist(const ov::String &query_string, const int32_t &track_id, int64_t msn, int64_t psn, bool skip, bool gzip, bool legacy, bool rewind) const
{
	auto chunklist = GetChunklistWriter(track_id);
	if (chunklist == nullptr)
	{
		logtw("Could not find chunklist for track_id = %d", track_id);
		return {RequestResult::NotFound, nullptr};
	}

	if (IsReadyToPlay() == false)
	{
		return {RequestResult::Accepted, nullptr};
	}

	if (msn >= 0 && psn >= 0)
	{
		int64_t last_msn, last_psn;
		if (chunklist->GetLastSequenceNumber(last_msn, last_psn) == false)
		{
			logtw("Could not get last sequence number for track_id = %d", track_id);
			return {RequestResult::NotFound, nullptr};
		}

		if (msn > last_msn || (msn >= last_msn && psn > last_psn))
		{
			// Hold the request until a Playlist contains a Segment with the requested Sequence Number
			logtd("Accepted chunklist for track_id = %d, msn = %ld, psn = %ld (last_msn = %ld, last_psn = %ld)", track_id, msn, psn, last_msn, last_psn);
			return {RequestResult::Accepted, nullptr};
		}
		else
		{
			logtd("Get chunklist for track_id = %d, msn = %ld, psn = %ld (last_msn = %ld, last_psn = %ld)", track_id, msn, psn, last_msn, last_psn);
		}
	}

	if (gzip == true)
	{
		return {RequestResult::Success, chunklist->ToGzipData(query_string, skip, legacy, rewind)};
	}

	return {RequestResult::Success, chunklist->ToString(query_string, skip, legacy, rewind).ToData(false)};
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetInitializationSegment(const int32_t &track_id) const
{
	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return {RequestResult::NotFound, nullptr};
	}

	return {RequestResult::Success, storage->GetInitializationSection()};
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetSegment(const int32_t &track_id, const int64_t &segment_number) const
{
	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return {RequestResult::NotFound, nullptr};
	}

	auto segment = storage->GetMediaSegment(segment_number);
	if (segment == nullptr)
	{
		logtw("Could not find segment for track_id = %d, segment = %ld (last_segment = %ld)", track_id, segment_number, storage->GetLastSegmentNumber());
		return {RequestResult::NotFound, nullptr};
	}

	return {RequestResult::Success, segment->GetData()};
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetChunk(const int32_t &track_id, const int64_t &segment_number, const int64_t &chunk_number) const
{
	logtd("LLHlsStream(%s) - GetChunk(%d, %ld, %ld)", GetName().CStr(), track_id, segment_number, chunk_number);

	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return {RequestResult::NotFound, nullptr};
	}

	auto [last_segment_number, last_chunk_number] = storage->GetLastChunkNumber();

	if ((segment_number > last_segment_number) || (segment_number == last_segment_number && chunk_number > last_chunk_number))
	{
		logtd("Accepted chunk for track_id = %d, segment = %ld, chunk = %ld (last_segment = %ld, last_chunk = %ld)", track_id, segment_number, chunk_number, last_segment_number, last_chunk_number);
		// Hold the request until a Playlist contains a Segment with the requested Sequence Number
		return {RequestResult::Accepted, nullptr};
	}
	else
	{
		logtd("Get chunk for track_id = %d, segment = %ld, chunk = %ld (last_segment = %ld, last_chunk = %ld)", track_id, segment_number, chunk_number, last_segment_number, last_chunk_number);
	}

	auto chunk = storage->GetMediaChunk(segment_number, chunk_number);
	if (chunk == nullptr)
	{
		logtw("Could not find partial segment for track_id = %d, segment = %ld, partial = %ld (last_segment = %ld, last_partial = %ld)", track_id, segment_number, chunk_number, last_segment_number, last_chunk_number);
		return {RequestResult::NotFound, nullptr};
	}

	return {RequestResult::Success, chunk->GetData()};
}

void LLHlsStream::BufferMediaPacketUntilReadyToPlay(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_initial_media_packet_buffer.Size() >= MAX_INITIAL_MEDIA_PACKET_BUFFER_SIZE)
	{
		// Drop the oldest packet, for OOM protection
		_initial_media_packet_buffer.Dequeue(0);
	}

	_initial_media_packet_buffer.Enqueue(media_packet);
}

bool LLHlsStream::SendBufferedPackets()
{
	logtd("SendBufferedPackets - BufferSize (%u)", _initial_media_packet_buffer.Size());
	while (_initial_media_packet_buffer.IsEmpty() == false)
	{
		auto buffered_media_packet = _initial_media_packet_buffer.Dequeue();
		if (buffered_media_packet.has_value() == false)
		{
			continue;
		}

		auto media_packet = buffered_media_packet.value();
		if (media_packet->GetMediaType() == cmn::MediaType::Data)
		{
			SendDataFrame(media_packet);
		}
		else
		{
			AppendMediaPacket(media_packet);
		}
	}

	return true;
}

void LLHlsStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (media_packet == nullptr || media_packet->GetData() == nullptr)
	{
		return;
	}

	if (GetState() == State::CREATED)
	{
		BufferMediaPacketUntilReadyToPlay(media_packet);
		return;
	}

	if (_initial_media_packet_buffer.IsEmpty() == false)
	{
		SendBufferedPackets();
	}

	AppendMediaPacket(media_packet);
}

void LLHlsStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (media_packet == nullptr || media_packet->GetData() == nullptr)
	{
		return;
	}

	if (GetState() == State::CREATED)
	{
		BufferMediaPacketUntilReadyToPlay(media_packet);
		return;
	}

	if (_initial_media_packet_buffer.IsEmpty() == false)
	{
		SendBufferedPackets();
	}

	AppendMediaPacket(media_packet);
}

void LLHlsStream::SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (media_packet->GetBitstreamFormat() != cmn::BitstreamFormat::ID3v2)
	{
		// Not supported
		return;
	}

	if (GetState() == State::CREATED)
	{
		BufferMediaPacketUntilReadyToPlay(media_packet);
		return;
	}

	if (_initial_media_packet_buffer.IsEmpty() == false)
	{
		SendBufferedPackets();
	}

	auto target_media_type = (media_packet->GetPacketType() == cmn::PacketType::VIDEO_EVENT) ? cmn::MediaType::Video : cmn::MediaType::Audio;

	for (const auto &it : GetTracks())
	{
		auto track = it.second;

		if (media_packet->GetPacketType() != cmn::PacketType::EVENT && track->GetMediaType() != target_media_type)
		{
			continue;
		}

		// Get Packager
		auto packager = GetPackager(track->GetId());
		if (packager == nullptr)
		{
			logtd("Could not find packager. track id: %d", track->GetId());
			continue;
		}
		logtd("AppendSample : track(%d) length(%d)", media_packet->GetTrackId(), media_packet->GetDataLength());

		packager->ReserveDataPacket(media_packet);
	}
}

bool LLHlsStream::AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto track = GetTrack(media_packet->GetTrackId());
	if (track == nullptr)
	{
		logtw("Could not find track. id: %d", media_packet->GetTrackId());
		return false;
	}

	if (IsSupportedCodec(track->GetCodecId()) == false)
	{
		return true;
	}

	// Get Packager
	auto packager = GetPackager(track->GetId());
	if (packager == nullptr)
	{
		logtw("Could not find packager. track id: %d", track->GetId());
		return false;
	}

	logtd("AppendSample : track(%d) length(%d)", media_packet->GetTrackId(), media_packet->GetDataLength());

	packager->AppendSample(media_packet);

	return true;
}

// Create and Get fMP4 packager with track info, storage and packager_config
bool LLHlsStream::AddPackager(const std::shared_ptr<const MediaTrack> &media_track, const std::shared_ptr<const MediaTrack> &data_track)
{
	auto cenc_property = _cenc_property;

	auto tag = ov::String::FormatString("%s/%s", GetApplicationInfo().GetName().CStr(), GetName().CStr());

	if (cenc_property.scheme != bmff::CencProtectScheme::None)
	{
		if (media_track->GetCodecId() == cmn::MediaCodecId::H264)
		{
			// Supported
		}
		else if (media_track->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			// Supported
		}
		else
		{
			cenc_property.scheme = bmff::CencProtectScheme::None;
			// Not yet support for other codec
			logte("LLHlsStream::AddPackager() - CENC is not supported for this codec(%s), this track will be excluded from CENC protection", StringFromMediaCodecId(media_track->GetCodecId()).CStr());
			_cenc_property.scheme = bmff::CencProtectScheme::None;
		}
	}

	// Create Storage
	auto storage = std::make_shared<bmff::FMP4Storage>(bmff::FMp4StorageObserver::GetSharedPtr(), media_track, _storage_config, tag);

	// Create fMP4 Packager
	_packager_config.cenc_property = cenc_property;
	auto packager = std::make_shared<bmff::FMP4Packager>(storage, media_track, data_track, _packager_config);

	// Create Initialization Segment
	if (packager->CreateInitializationSegment() == false)
	{
		logtc("LLHlsStream::AddPackager() - Failed to create initialization segment");
		return false;
	}

	// milliseconds to seconds
	auto segment_count = _storage_config.max_segments;
	auto segment_duration = static_cast<float_t>(_storage_config.segment_duration_ms) / 1000.0;
	auto chunk_duration = static_cast<float_t>(_packager_config.chunk_duration_ms) / 1000.0;
	auto track_id = media_track->GetId();

	auto chunklist = std::make_shared<LLHlsChunklist>(GetChunklistName(track_id),
													  GetTrack(track_id),
													  segment_count,
													  segment_duration,
													  chunk_duration,
													  GetInitializationSegmentName(track_id),
													  _preload_hint_enabled);

	if (cenc_property.scheme != bmff::CencProtectScheme::None)
	{
		chunklist->EnableCenc(cenc_property);
	}

	{
		std::lock_guard<std::shared_mutex> storage_lock(_storage_map_lock);
		_storage_map.emplace(media_track->GetId(), storage);
	}

	{
		std::lock_guard<std::shared_mutex> packager_lock(_packager_map_lock);
		_packager_map.emplace(media_track->GetId(), packager);
	}

	{
		std::unique_lock<std::shared_mutex> lock(_chunklist_map_lock);
		_chunklist_map.emplace(track_id, chunklist);
	}

	return true;
}

// Get storage with the track id
std::shared_ptr<bmff::FMP4Storage> LLHlsStream::GetStorage(const int32_t &track_id) const
{
	std::shared_lock<std::shared_mutex> lock(_storage_map_lock);
	auto it = _storage_map.find(track_id);
	if (it == _storage_map.end())
	{
		return nullptr;
	}

	return it->second;
}

// Get fMP4 packager with the track id
std::shared_ptr<bmff::FMP4Packager> LLHlsStream::GetPackager(const int32_t &track_id) const
{
	std::shared_lock<std::shared_mutex> lock(_packager_map_lock);
	auto it = _packager_map.find(track_id);
	if (it == _packager_map.end())
	{
		return nullptr;
	}

	return it->second;
}

std::shared_ptr<LLHlsChunklist> LLHlsStream::GetChunklistWriter(const int32_t &track_id) const
{
	std::shared_lock<std::shared_mutex> lock(_chunklist_map_lock);
	auto it = _chunklist_map.find(track_id);
	if (it == _chunklist_map.end())
	{
		return nullptr;
	}

	return it->second;
}

ov::String LLHlsStream::GetChunklistName(const int32_t &track_id) const
{
	// chunklist_<track id>_<media type>_<stream key>_llhls.m3u8
	return ov::String::FormatString("chunklist_%d_%s_%s_llhls.m3u8",
									track_id,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

ov::String LLHlsStream::GetInitializationSegmentName(const int32_t &track_id) const
{
	// init_<track id>_<media type>_<random str>_llhls.m4s
	return ov::String::FormatString("init_%d_%s_%s_llhls.m4s",
									track_id,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

ov::String LLHlsStream::GetSegmentName(const int32_t &track_id, const int64_t &segment_number) const
{
	// seg_<track id>_<segment number>_<media type>_<random str>_llhls.m4s
	return ov::String::FormatString("seg_%d_%lld_%s_%s_llhls.m4s",
									track_id,
									segment_number,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

ov::String LLHlsStream::GetPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number) const
{
	// part_<track id>_<segment number>_<partial number>_<media type>_<random str>_llhls.m4s
	return ov::String::FormatString("part_%d_%lld_%lld_%s_%s_llhls.m4s",
									track_id,
									segment_number,
									partial_number,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

ov::String LLHlsStream::GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number, bool last_chunk) const
{
	auto next_segment_number = 0;
	auto next_partial_number = 0;

	if (last_chunk == true)
	{
		next_segment_number = segment_number + 1;
		next_partial_number = 0;
	}
	else
	{
		next_segment_number = segment_number;
		next_partial_number = partial_number + 1;
	}

	// part_<track id>_<segment number>_<partial number>_<media type>_<random str>_llhls.m4s
	return ov::String::FormatString("part_%d_%lld_%lld_%s_%s_llhls.m4s",
									track_id,
									next_segment_number,
									next_partial_number,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

void LLHlsStream::OnFMp4StorageInitialized(const int32_t &track_id)
{
	// Not to do anything
}

bool LLHlsStream::IsReadyToPlay() const
{
	return _playlist_ready;
}

bool LLHlsStream::CheckPlaylistReady()
{
	// lock
	std::lock_guard<std::shared_mutex> lock(_playlist_ready_lock);
	if (_playlist_ready == true)
	{
		return true;
	}

	std::shared_lock<std::shared_mutex> storage_lock(_storage_map_lock);

	for (const auto &[track_id, storage] : _storage_map)
	{
		// At least one segment must be created.
		if (storage->GetLastSegmentNumber() < 0)
		{
			return false;
		}

		_max_chunk_duration_ms = std::max(_max_chunk_duration_ms, storage->GetMaxChunkDurationMs());
		_min_chunk_duration_ms = std::min(_min_chunk_duration_ms, storage->GetMinChunkDurationMs());
	}

	storage_lock.unlock();

	std::shared_lock<std::shared_mutex> chunklist_lock(_chunklist_map_lock);

	double min_part_hold_back = (static_cast<double>(_max_chunk_duration_ms) / 1000.0f) * 3.0f;
	double final_part_hold_back = std::max(min_part_hold_back, _configured_part_hold_back);
	for (const auto &[track_id, chunklist] : _chunklist_map)
	{
		chunklist->SetPartHoldBack(final_part_hold_back);

		DumpInitSegmentOfAllItems(chunklist->GetTrack()->GetId());
	}

	chunklist_lock.unlock();

	_playlist_ready = true;

	// Dump master playlist if configured
	DumpMasterPlaylistsOfAllItems();

	return true;
}

void LLHlsStream::OnMediaSegmentCreated(const int32_t &track_id, const uint32_t &segment_number)
{
	// Check whether at least one segment of every track has been created.
	CheckPlaylistReady();

	auto playlist = GetChunklistWriter(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logte("Storage is not found : stream = %s, track_id = %d", GetName().CStr(), track_id);
		return;
	}

	auto segment = storage->GetMediaSegment(segment_number);
	if (segment == nullptr)
	{
		logte("Segment is not found : stream = %s, track_id = %d, segment_number = %u", GetName().CStr(), track_id, segment_number);
		return;
	}

	// Empty segment
	auto segment_info = LLHlsChunklist::SegmentInfo(segment->GetNumber(), GetSegmentName(track_id, segment->GetNumber()));

	playlist->CreateSegmentInfo(segment_info);

	logtd("Media segment updated : track_id = %d, segment_number = %d", track_id, segment_number);
}

void LLHlsStream::OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number, bool last_chunk)
{
	auto playlist = GetChunklistWriter(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logte("Storage is not found : stream = %s, track_id = %d", GetName().CStr(), track_id);
		return;
	}

	auto chunk = storage->GetMediaChunk(segment_number, chunk_number);
	if (chunk == nullptr)
	{
		logte("Chunk is not found : stream = %s, track_id = %d, segment_number = %u, chunk_number = %u", GetName().CStr(), track_id, segment_number, chunk_number);
		return;
	}

	// Milliseconds
	auto chunk_duration = static_cast<float>(chunk->GetDuration()) / static_cast<float>(1000.0);

	// Human readable timestamp
	if (_first_chunk == true)
	{
		_first_chunk = false;

		auto first_chunk_timestamp_ms = (static_cast<float>(chunk->GetStartTimestamp()) / GetTrack(track_id)->GetTimeBase().GetTimescale()) * 1000.0;

		_wallclock_offset_ms = std::chrono::duration_cast<std::chrono::milliseconds>(GetInputStreamPublishedTime().time_since_epoch()).count() - first_chunk_timestamp_ms;
	}

	auto start_timestamp = (static_cast<float>(chunk->GetStartTimestamp()) / GetTrack(track_id)->GetTimeBase().GetTimescale()) * 1000.0;
	start_timestamp += _wallclock_offset_ms;

	auto chunk_info = LLHlsChunklist::SegmentInfo(chunk->GetNumber(), start_timestamp, chunk_duration, chunk->GetSize(),
												  GetPartialSegmentName(track_id, segment_number, chunk->GetNumber()),
												  GetNextPartialSegmentName(track_id, segment_number, chunk->GetNumber(), last_chunk),
												  chunk->IsIndependent(), last_chunk);

	playlist->AppendPartialSegmentInfo(segment_number, chunk_info);

	logtd("Media chunk updated : track_id = %d, segment_number = %d, chunk_number = %d, start_timestamp = %llu, chunk_duration = %f", track_id, segment_number, chunk_number, chunk->GetStartTimestamp(), chunk_duration);

	// Notify
	NotifyPlaylistUpdated(track_id, segment_number, chunk_number);

	if (last_chunk == true)
	{
		DumpSegmentOfAllItems(track_id, segment_number);
	}
}

void LLHlsStream::OnMediaSegmentDeleted(const int32_t &track_id, const uint32_t &segment_number)
{
	auto playlist = GetChunklistWriter(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	playlist->RemoveSegmentInfo(segment_number);

	logtd("Media segment deleted : track_id = %d, segment_number = %d", track_id, segment_number);
}

void LLHlsStream::NotifyPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part)
{
	// Make std::any for broadcast
	// I think make_shared is better than copy sizeof(PlaylistUpdatedEvent) to all sessions
	auto event = std::make_shared<PlaylistUpdatedEvent>(track_id, msn, part);
	auto notification = std::make_any<std::shared_ptr<PlaylistUpdatedEvent>>(event);
	BroadcastPacket(notification);
}

int64_t LLHlsStream::GetMinimumLastSegmentNumber() const
{
	// lock storage map
	std::shared_lock<std::shared_mutex> storage_lock(_storage_map_lock);
	int64_t min_segment_number = std::numeric_limits<int64_t>::max();
	for (const auto &it : _storage_map)
	{
		auto storage = it.second;
		if (storage == nullptr)
		{
			continue;
		}

		auto segment_number = storage->GetLastSegmentNumber();
		if (segment_number < min_segment_number)
		{
			min_segment_number = segment_number;
		}
	}

	return min_segment_number;
}

std::tuple<bool, ov::String> LLHlsStream::StartDump(const std::shared_ptr<info::Dump> &info)
{
	std::lock_guard<std::shared_mutex> lock(_dumps_lock);

	for (const auto &it : _dumps)
	{
		// Check duplicate ID
		if (it.second->GetId() == info->GetId())
		{
			return {false, "Duplicate ID"};
		}

		// Check duplicate infoFile
		if ((it.second->GetInfoFileUrl().IsEmpty() == false) && it.second->GetInfoFileUrl() == info->GetInfoFileUrl())
		{
			return {false, "Duplicate info file"};
		}
	}

	auto dump_info = std::make_shared<mdl::Dump>(info);
	dump_info->SetEnabled(true);

	// lock playlist ready
	std::shared_lock<std::shared_mutex> lock_playlist_ready(_playlist_ready_lock);
	if (IsReadyToPlay() == false)
	{
		// If the playlist is not ready, add it to the queue and wait for the playlist to be ready.
		// It will work when the playlist is ready (CheckPlaylistReady()).
		_dumps.emplace(dump_info->GetId(), dump_info);
		return {true, ""};
	}
	lock_playlist_ready.unlock();

	// Dump Init Segment for all tracks
	std::shared_lock<std::shared_mutex> storage_lock(_storage_map_lock);
	auto storage_map = _storage_map;
	storage_lock.unlock();

	// Find minimum segment number
	int64_t min_segment_number = GetMinimumLastSegmentNumber();

	logti("Start dump requested: stream_name = %s, dump_id = %s, min_segment_number = %d", GetName().CStr(), dump_info->GetId().CStr(), min_segment_number);

	for (const auto &it : storage_map)
	{
		auto track_id = it.first;
		if (DumpInitSegment(dump_info, track_id) == false)
		{
			return {false, "Could not dump init segment"};
		}

		// Dump from min_segment_number to last segment
		auto storage = it.second;
		if (storage == nullptr)
		{
			continue;
		}

		auto last_segment_number = storage->GetLastSegmentNumber();
		for (int64_t segment_number = min_segment_number; segment_number <= last_segment_number; segment_number++)
		{
			if (DumpSegment(dump_info, track_id, segment_number) == false)
			{
				return {false, "Could not dump segment"};
			}

			logti("Dump base segment : stream_name = %s, dump_id = %s, track_id = %d, segment_number = %d, min_segment_number = %d, last_segment_number = %d", GetName().CStr(), dump_info->GetId().CStr(), track_id, segment_number, min_segment_number, last_segment_number);
		}
	}

	// Dump Master Playlist
	if (DumpMasterPlaylist(dump_info) == false)
	{
		StopToSaveOldSegmentsInfo();
		return {false, "Could not dump master playlist"};
	}

	_dumps.emplace(dump_info->GetId(), dump_info);

	return {true, ""};
}

std::tuple<bool, ov::String> LLHlsStream::StopDump(const std::shared_ptr<info::Dump> &dump_info)
{
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);

	if (dump_info->GetId().IsEmpty() == false)
	{
		auto it = _dumps.find(dump_info->GetId());
		if (it == _dumps.end())
		{
			return {false, "Could not find dump info"};
		}
		auto dump_item = it->second;
		dump_item->SetEnabled(false);
		dump_item->CompleteDump();
	}
	// All stop
	else
	{
		for (const auto &it : _dumps)
		{
			auto dump_item = it.second;
			dump_item->SetEnabled(false);
			dump_item->CompleteDump();
		}
	}

	StopToSaveOldSegmentsInfo();

	lock.unlock();

	return {true, ""};
}

// It must be called in the lock of _dumps_lock
bool LLHlsStream::StopToSaveOldSegmentsInfo()
{
	// check if all dumps are disabled
	bool all_disabled = true;
	for (const auto &it : _dumps)
	{
		auto dump_item = it.second;
		if (dump_item->IsEnabled())
		{
			all_disabled = false;
			break;
		}
	}

	if (all_disabled == true)
	{
		// stop to keep old segments in _chunklist_map
		// shared lock
		std::shared_lock<std::shared_mutex> chunk_lock(_chunklist_map_lock);
		for (const auto &it : _chunklist_map)
		{
			auto chunklist = it.second;
			chunklist->SaveOldSegmentInfo(false);
		}
	}

	return true;
}

// Get dump info
std::shared_ptr<const mdl::Dump> LLHlsStream::GetDumpInfo(const ov::String &dump_id)
{
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);
	auto it = _dumps.find(dump_id);
	if (it == _dumps.end())
	{
		return nullptr;
	}
	return it->second;
}

// Get dumps
std::vector<std::shared_ptr<const mdl::Dump>> LLHlsStream::GetDumpInfoList()
{
	std::vector<std::shared_ptr<const mdl::Dump>> dump_list;
	std::shared_lock<std::shared_mutex> lock(_dumps_lock);
	for (const auto &it : _dumps)
	{
		dump_list.push_back(it.second);
	}
	return dump_list;
}