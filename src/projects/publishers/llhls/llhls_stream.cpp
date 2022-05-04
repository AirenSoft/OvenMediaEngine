//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "base/publisher/application.h"
#include "base/publisher/stream.h"

#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsStream> LLHlsStream::Create(const std::shared_ptr<pub::Application> application, const info::Stream &info)
{
	auto stream = std::make_shared<LLHlsStream>(application, info);
	return stream;
}

LLHlsStream::LLHlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info)
	: Stream(application, info)
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

	logtd("LLHlsStream(%ld) has been started", GetId());
	
	_packager_config.chunk_duration_ms = 1000;
	_storage_config.max_segments = 10;
	_storage_config.segment_duration_ms = 5000;

	for (const auto &[id, track] : _tracks)
	{
		if ( (track->GetCodecId() == cmn::MediaCodecId::H264) || 
			 (track->GetCodecId() == cmn::MediaCodecId::Aac) )
		{
			if (AddPackager(track) == false)
			{
				logte("LLHlsStream(%s/%s) - Failed to add packager for track(%ld)", GetApplication()->GetName().CStr(), GetName().CStr(), track->GetId());
				return false;
			}
		}
		else 
		{
			logti("LLHlsStream(%s/%s) - Ignore unsupported codec(%s)", GetApplication()->GetName().CStr(), GetName().CStr(), StringFromMediaCodecId(track->GetCodecId()).CStr());
			continue;
		}
	}

	return Stream::Start();
}

bool LLHlsStream::Stop()
{
	logtd("LLHlsStream(%u) has been stopped", GetId());

	// clear all packagers
	std::lock_guard<std::shared_mutex> lock(_packager_map_lock);
	_packager_map.clear();

	// clear all storages
	std::lock_guard<std::shared_mutex> lock2(_storage_map_lock);
	_storage_map.clear();

	// clear all playlist
	std::lock_guard<std::shared_mutex> lock3(_playlist_map_lock);
	_playlist_map.clear();

	return Stream::Stop();
}

void LLHlsStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		return;
	}

	AppendMediaPacket(media_packet);
}

void LLHlsStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		return;
	}

	AppendMediaPacket(media_packet);
}

bool LLHlsStream::AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto track = GetTrack(media_packet->GetTrackId());
	if (track == nullptr)
	{
		logtw("Could not find track. id: %d", media_packet->GetTrackId());
		return false;;
	}

	if ( (track->GetCodecId() == cmn::MediaCodecId::H264) || 
			 (track->GetCodecId() == cmn::MediaCodecId::Aac) )
	{
		// Get Packager
		auto packager = GetPackager(track->GetId());
		if (packager == nullptr)
		{
			logtw("Could not find packager. track id: %d", track->GetId());
			return false;
		}

		logtd("AppendSample : track(%d) length(%d)", media_packet->GetTrackId(), media_packet->GetDataLength());

		packager->AppendSample(media_packet);
	}

	return true;
}

// Create and Get fMP4 packager with track info, storage and packager_config
bool LLHlsStream::AddPackager(const std::shared_ptr<const MediaTrack> &track)
{
	// Create Storage
	auto storage = std::make_shared<bmff::FMP4Storage>(bmff::FMp4StorageObserver::GetSharedPtr(), track, _storage_config);

	// Create fMP4 Packager
	auto packager = std::make_shared<bmff::FMP4Packager>(storage, track, _packager_config);

	// Create Initialization Segment
	if (packager->CreateInitializationSegment() == false)
	{
		logtc("LLHlsStream::AddPackager() - Failed to create initialization segment");
		return false;
	}
	
	// lock
	std::lock_guard<std::shared_mutex> storage_lock(_storage_map_lock);
	std::lock_guard<std::shared_mutex> packager_lock(_packager_map_lock);

	// Add to map
	_storage_map.emplace(track->GetId(), storage);
	_packager_map.emplace(track->GetId(), packager);

	return true;
}

// Get storage with the track id
std::shared_ptr<bmff::FMP4Storage> LLHlsStream::GetStorage(const int32_t &track_id)
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
std::shared_ptr<bmff::FMP4Packager> LLHlsStream::GetPackager(const int32_t &track_id)
{
	std::shared_lock<std::shared_mutex> lock(_packager_map_lock);
	auto it = _packager_map.find(track_id);
	if (it == _packager_map.end())
	{
		return nullptr;
	}

	return it->second;
}

std::shared_ptr<LLHlsPlaylist> LLHlsStream::GetPlaylist(const int32_t &track_id)
{
	std::shared_lock<std::shared_mutex> lock(_playlist_map_lock);
	auto it = _playlist_map.find(track_id);
	if (it == _playlist_map.end())
	{
		return nullptr;
	}

	return it->second;
}

ov::String LLHlsStream::GetPlaylistName()
{
	// llhls.m3u8
	return ov::String::FormatString("llhls.m3u8");
}

ov::String LLHlsStream::GetChunklistName(const int32_t &track_id)
{
	// chunklist_<track id>_<media type>_llhls.m3u8
	return ov::String::FormatString("chunklist_%d_%s_llhls.m3u8",
										track_id, 
										StringFromMediaType(GetTrack(track_id)->GetMediaType()).CStr());
}

ov::String LLHlsStream::GetIntializationSegmentName(const int32_t &track_id)
{
	// init_<track id>_<media type>_llhls.m4s
	return ov::String::FormatString("init_%d_%s_llhls.m4s",
									track_id,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr());
}

ov::String LLHlsStream::GetSegmentName(const int32_t &track_id, const int64_t &segment_number)
{
	// seg_<track id>_<segment number>_<media type>_llhls.m4s
	return ov::String::FormatString("seg_%d_%lld_%s_llhls.m4s", 
									track_id,
									segment_number,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr());
}

ov::String LLHlsStream::GetPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number)
{
	// part_<track id>_<segment number>_<partial number>_<media type>_llhls.m4s
	return ov::String::FormatString("part_%d_%lld_%lld_%s_llhls.m4s", 
									track_id,
									segment_number,
									partial_number,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr());
}

ov::String LLHlsStream::GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number)
{
	// part_<track id>_<segment number>_<partial number>_<media type>_llhls.m4s
	return ov::String::FormatString("part_%d_%lld_%lld_%s_llhls.m4s", 
									track_id,
									segment_number,
									partial_number + 1,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr());
}

bool LLHlsStream::ParseFileName(const ov::String &file_name, FileType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number)
{
	// Split the querystring if it has a '?'
	auto name_query_items = file_name.Split("?");
	
	// Split to filename.ext
	auto name_ext_items = name_query_items[0].Split(".");
	if (name_ext_items.size() != 2 || name_ext_items[1] != "m4s" || name_ext_items[1] != "m3u8")
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	// Split to <file type>_<track id>_<segment number>_<partial number>
	auto name_items = name_ext_items[0].Split("_");
	if (name_items[0] == "llhls")
	{
		if (name_ext_items[1] != "m3u8")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Playlist;
	}
	else if (name_items[0] == "chunklist")
	{
		// chunklist_<track id>_<media type>_llhls.m3u8
		if (name_items.size() != 4 || name_ext_items[1] != "m3u8")
		{
			logtw("Invalid chunklist file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Chunklist;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
	}
	else if (name_items[0] == "init")
	{
		// init_<track id>_<media type>
		if (name_items.size() != 3 || name_ext_items[1] != "m4s")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::InitializationSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
	}
	else if (name_items[0] == "seg" || name_ext_items[1] != "m4s")
	{
		// seg_<track id>_<segment number>_<media type>
		if (name_items.size() != 4)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Segment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
	}
	else if (name_items[0] == "part" || name_ext_items[1] != "m4s")
	{
		// part_<track id>_<segment number>_<partial number>_<media type>
		if (name_items.size() != 5)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::PartialSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
		partial_number = ov::Converter::ToInt64(name_items[3].CStr());
	}
	else
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	return true;
}

void LLHlsStream::OnFMp4StorageInitialized(const int32_t &track_id)
{
	// milliseconds to seconds
	auto segment_duration = static_cast<float_t>(_storage_config.segment_duration_ms) / 1000.0;
	auto chunk_duration = static_cast<float_t>(_packager_config.chunk_duration_ms) / 1000.0;

	auto playlist = std::make_shared<LLHlsPlaylist>(GetTrack(track_id),
													_storage_config.max_segments, 
													segment_duration, 
													chunk_duration, 
													GetIntializationSegmentName(track_id));

	std::unique_lock<std::shared_mutex> lock(_playlist_map_lock);
	_playlist_map[track_id] = playlist;
}

void LLHlsStream::OnMediaSegmentUpdated(const int32_t &track_id, const uint32_t &segment_number)
{
	auto playlist = GetPlaylist(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto segment = GetStorage(track_id)->GetMediaSegment(segment_number);

	// Timescale to seconds(demical)
	auto segment_duration = static_cast<float>(segment->GetDuration()) / static_cast<float>(GetTrack(track_id)->GetTimeBase().GetTimescale());

	auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(GetCreatedTime().time_since_epoch()).count() + segment->GetStartTimestamp();

	auto segment_info = LLHlsPlaylist::SegmentInfo(segment->GetNumber(), start_timestamp, segment_duration,
													segment->GetSize(), GetSegmentName(track_id, segment->GetNumber()), "", true);

	playlist->AppendSegmentInfo(segment_info);

	logti("Media segment updated : track_id = %d, segment_number = %d, start_timestamp = %llu, segment_duration = %f", track_id, segment_number, segment->GetStartTimestamp(), segment_duration);
	
	// Output playlist
	logti("[Playlist]\n%s", playlist->ToString().CStr());
}

void LLHlsStream::OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number)
{
	auto playlist = GetPlaylist(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto chunk = GetStorage(track_id)->GetMediaChunk(segment_number, chunk_number);
	// Timescale to seconds(demical)
	auto chunk_duration = static_cast<float>(chunk->GetDuration()) / static_cast<float>(GetTrack(track_id)->GetTimeBase().GetTimescale());

	// Human readable timestamp
	auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(GetCreatedTime().time_since_epoch()).count() + chunk->GetStartTimestamp();

	auto chunk_info = LLHlsPlaylist::SegmentInfo(chunk->GetNumber(), start_timestamp, chunk_duration, chunk->GetSize(), 
												GetPartialSegmentName(track_id, segment_number, chunk->GetNumber()), 
												GetNextPartialSegmentName(track_id, segment_number, chunk->GetNumber()), chunk->IsIndependent());

	playlist->AppendPartialSegmentInfo(segment_number, chunk_info);

	logti("Media chunk updated : track_id = %d, segment_number = %d, chunk_number = %d, start_timestamp = %llu, chunk_duration = %f", track_id, segment_number, chunk_number, chunk->GetStartTimestamp(), chunk_duration);

	// Output playlist
	logti("[Playlist]\n%s", playlist->ToString().CStr());
}