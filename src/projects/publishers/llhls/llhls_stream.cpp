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

	logtd("LLHlsStream(%ld) has been started", GetId());
	
	_packager_config.chunk_duration_ms = 1000;
	_storage_config.max_segments = 10;
	_storage_config.segment_duration_ms = 5000;

	//TODO(Getroot): It will be replaced with ABR config
	std::shared_ptr<MediaTrack> first_video_track = nullptr, first_audio_track = nullptr;

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

			// Add EXT-X-MEDIA
			AddMediaCandidateToMasterPlaylist(track);

			if ( first_video_track == nullptr && track->GetMediaType() == cmn::MediaType::Video )
			{
				first_video_track = track;
			}
			else if ( first_audio_track == nullptr && track->GetMediaType() == cmn::MediaType::Audio )
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

	//TODO(Getroot): It will be replaced with ABR config
	AddStreamInfToMasterPlaylist(first_video_track, first_audio_track);

	logti("Master Playlist : %s", _master_playlist.ToString().CStr());

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
	
	{
		std::lock_guard<std::shared_mutex> storage_lock(_storage_map_lock);
		_storage_map.emplace(track->GetId(), storage);
	}

	{
		std::lock_guard<std::shared_mutex> packager_lock(_packager_map_lock);
		_packager_map.emplace(track->GetId(), packager);
	}

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

	logtd("Media segment updated : track_id = %d, segment_number = %d, start_timestamp = %llu, segment_duration = %f", track_id, segment_number, segment->GetStartTimestamp(), segment_duration);
	
	// Output playlist
	logtd("[Playlist]\n%s", playlist->ToString().CStr());
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

	logtd("Media chunk updated : track_id = %d, segment_number = %d, chunk_number = %d, start_timestamp = %llu, chunk_duration = %f", track_id, segment_number, chunk_number, chunk->GetStartTimestamp(), chunk_duration);

	// Output playlist
	logtd("[Playlist]\n%s", playlist->ToString().CStr());
}

// Add X-MEDIA to the master playlist
void LLHlsStream::AddMediaCandidateToMasterPlaylist(const std::shared_ptr<const MediaTrack> &track)
{
	LLHlsMasterPlaylist::MediaInfo media_info;

	media_info._type = track->GetMediaType()==cmn::MediaType::Video ? LLHlsMasterPlaylist::MediaInfo::Type::Video : LLHlsMasterPlaylist::MediaInfo::Type::Audio;
	media_info._group_id = ov::Converter::ToString(track->GetId()); // Currently there is only one element per group.
	media_info._name = "none";
	media_info._default = true; // There is no group media so all track is default
	media_info._uri = GetChunklistName(track->GetId());
	media_info._track = track;

	_master_playlist.AddGroupMedia(media_info);
}

// Add X-STREAM-INF to the master playlist
void LLHlsStream::AddStreamInfToMasterPlaylist(const std::shared_ptr<const MediaTrack> &video_track, const std::shared_ptr<const MediaTrack> &audio_track)
{
	LLHlsMasterPlaylist::StreamInfo stream_info;
	if (video_track != nullptr)
	{
		stream_info._track = video_track;
		stream_info._uri = GetChunklistName(video_track->GetId());
		
		if (audio_track != nullptr)
		{
			stream_info._audio_group_id = ov::Converter::ToString(audio_track->GetId());
		}
	}
	else
	{
		stream_info._track = audio_track;
		stream_info._uri = GetChunklistName(audio_track->GetId());
	}

	_master_playlist.AddStreamInfo(stream_info);
}