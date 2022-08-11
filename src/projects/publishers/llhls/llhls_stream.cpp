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

#include "llhls_application.h"
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
	_stream_key = ov::Random::GenerateString(8);
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

	_packager_config.chunk_duration_ms = llhls_config.GetChunkDuration() * 1000.0;
	_storage_config.max_segments = llhls_config.GetSegmentCount();
	_storage_config.segment_duration_ms = llhls_config.GetSegmentDuration() * 1000;

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

			// For default llhls.m3u8
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

	if (first_video_track == nullptr && first_audio_track == nullptr)
	{
		logtw("LLHLS stream [%s/%s] could not be created because there is no supported codec.", GetApplication()->GetName().CStr(), GetName().CStr());
		return false;
	}

	// If there is no default playlist, make default playlist
	// Default playlist is consist of first compatible video and audio track among all tracks
	ov::String defautl_playlist_name = DEFAULT_PLAYLIST_NAME;
	auto default_playlist_name_without_ext = defautl_playlist_name.Substring(0, defautl_playlist_name.IndexOfRev('.'));
	auto default_playlist = Stream::GetPlaylist(default_playlist_name_without_ext);
	if (default_playlist == nullptr)
	{
		auto playlist = std::make_shared<info::Playlist>("default", default_playlist_name_without_ext);
		auto rendition = std::make_shared<info::Rendition>("default", first_video_track?first_video_track->GetName():"", first_audio_track?first_audio_track->GetName():"");

		playlist->AddRendition(rendition);
		auto master_playlist = CreateMasterPlaylist(playlist);

		std::lock_guard<std::mutex> guard(_master_playlists_lock);
		_master_playlists[defautl_playlist_name] = master_playlist;
	}

	logti("LLHlsStream has been created : %s/%u\nOriginMode(%s) Chunk Duration(%.2f) Segment Duration(%u) Segment Count(%u)", GetName().CStr(), GetId(), 
			ov::Converter::ToString(llhls_config.IsOriginMode()).CStr(), llhls_config.GetChunkDuration(), llhls_config.GetSegmentDuration(), llhls_config.GetSegmentCount());

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
	std::lock_guard<std::shared_mutex> lock3(_chunklist_map_lock);
	_chunklist_map.clear();

	return Stream::Stop();
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

	// Add all media candidates to master playlist
	for (const auto &[track_id, track] : GetTracks())
	{
		if ( (track->GetCodecId() != cmn::MediaCodecId::H264) && 
				(track->GetCodecId() != cmn::MediaCodecId::Aac) )
		{
			continue;
		}

		// Now there is no group in tracks, it will be supported in the future
		auto group_id = ov::Converter::ToString(track_id);
		master_playlist->AddMediaCandidateToMasterPlaylist(group_id, track, GetChunklistName(track_id));
	}

	// Add stream 
	for (const auto &rendition : playlist->GetRenditionList())
	{
		auto video_track = GetTrack(rendition->GetVideoTrackName());
		auto video_chunklist_name = video_track ? GetChunklistName(video_track->GetId()) : "";
		auto audio_track = GetTrack(rendition->GetAudioTrackName());
		auto audio_chunklist_name = audio_track ? GetChunklistName(audio_track->GetId()) : "";

		if ( (video_track != nullptr && video_track->GetCodecId() != cmn::MediaCodecId::H264) || 
			(audio_track != nullptr && audio_track->GetCodecId() != cmn::MediaCodecId::Aac) )
		{
			logtw("LLHlsStream(%s/%s) - Exclude the rendition(%s) from the %s.m3u8 due to unsupported codec", GetApplication()->GetName().CStr(), GetName().CStr(), 
							rendition->GetName().CStr(), playlist->GetFileName().CStr());
			continue;
		}

		master_playlist->AddStreamInfToMasterPlaylist(video_track, video_chunklist_name, audio_track, audio_chunklist_name);
	}

	return master_playlist;
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<const ov::Data>> LLHlsStream::GetMasterPlaylist(const ov::String &file_name, const ov::String &chunk_query_string, bool gzip, bool legacy)
{
	if (GetState() != State::STARTED)
	{
		return { RequestResult::NotFound, nullptr };
	}

	if (_playlist_ready == false)
	{
		return { RequestResult::Accepted, nullptr };
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
			return { RequestResult::NotFound, nullptr };
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
		return { RequestResult::NotFound, nullptr };
	}

	if (gzip == true)
	{
		return { RequestResult::Success, master_playlist->ToGzipData(chunk_query_string, legacy) };
	}

	return { RequestResult::Success, master_playlist->ToString(chunk_query_string, legacy).ToData(false) };
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<const ov::Data>> LLHlsStream::GetChunklist(const ov::String &query_string,const int32_t &track_id, int64_t msn, int64_t psn, bool skip, bool gzip, bool legacy) const
{
	auto chunklist = GetChunklistWriter(track_id);
	if (chunklist == nullptr)
	{
		logtw("Could not find chunklist for track_id = %d", track_id);
		return { RequestResult::NotFound, nullptr };
	}

	if (_playlist_ready == false)
	{
		return { RequestResult::Accepted, nullptr };
	}

	if (msn >= 0 && psn >= 0)
	{
		int64_t last_msn, last_psn;
		if (chunklist->GetLastSequenceNumber(last_msn, last_psn) == false)
		{
			logtw("Could not get last sequence number for track_id = %d", track_id);
			return { RequestResult::NotFound, nullptr };
		}

		if (msn > last_msn || (msn >= last_msn && psn > last_psn))
		{
			// Hold the request until a Playlist contains a Segment with the requested Sequence Number
			return { RequestResult::Accepted, nullptr };
		}
	}

	// lock
	std::shared_lock<std::shared_mutex> lock(_chunklist_map_lock);
	if (gzip == true)
	{
		return { RequestResult::Success, chunklist->ToGzipData(query_string, _chunklist_map, skip, legacy) };
	}

	return { RequestResult::Success, chunklist->ToString(query_string, _chunklist_map, skip, legacy).ToData(false) };
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetInitializationSegment(const int32_t &track_id) const
{
	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return { RequestResult::NotFound, nullptr };
	}

	return { RequestResult::Success, storage->GetInitializationSection() };
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetSegment(const int32_t &track_id, const int64_t &segment_number) const
{
	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return { RequestResult::NotFound, nullptr };
	}

	auto segment = storage->GetMediaSegment(segment_number);
	if (segment == nullptr)
	{
		logtw("Could not find segment for track_id = %d, segment_number = %ld", track_id, segment_number);
		return { RequestResult::NotFound, nullptr };
	}

	return { RequestResult::Success, storage->GetMediaSegment(segment_number)->GetData() };
}

std::tuple<LLHlsStream::RequestResult, std::shared_ptr<ov::Data>> LLHlsStream::GetChunk(const int32_t &track_id, const int64_t &segment_number, const int64_t &chunk_number) const
{
	logtd("LLHlsStream(%s) - GetChunk(%d, %ld, %ld)", GetName().CStr(), track_id, segment_number, chunk_number);

	auto storage = GetStorage(track_id);
	if (storage == nullptr)
	{
		logtw("Could not find storage for track_id = %d", track_id);
		return { RequestResult::NotFound, nullptr };
	}

	auto [last_segment_number, last_chunk_number] = storage->GetLastChunkNumber();

	if (segment_number == last_segment_number && chunk_number > last_chunk_number)
	{
		// Hold the request until a Playlist contains a Segment with the requested Sequence Number
		return { RequestResult::Accepted, nullptr };
	}
	else if (segment_number > last_segment_number)
	{
		// Not Found
		logtw("Could not find segment for track_id = %d, segment_number = %ld (last_segemnt = %ld)", track_id, segment_number, last_segment_number);
		return { RequestResult::NotFound, nullptr };
	}

	auto chunk = storage->GetMediaChunk(segment_number, chunk_number);
	if (chunk == nullptr)
	{
		logtw("Could not find segment for track_id = %d, segment_number = %ld, partial_number = %ld", track_id, segment_number, chunk_number);
		return { RequestResult::NotFound, nullptr };
	}

	return { RequestResult::Success, chunk->GetData() };
}

void LLHlsStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		_initial_media_packet_buffer.Enqueue(media_packet);
		return;
	}

	if (_initial_media_packet_buffer.IsEmpty() == false)
	{
		logtd("SendVideoFrame() - BufferSize (%u)", _initial_media_packet_buffer.Size());
		while (_initial_media_packet_buffer.IsEmpty() == false)
		{
			auto buffered_media_packet = _initial_media_packet_buffer.Dequeue();
			if (buffered_media_packet.has_value() == false)
			{
				continue;
			}

			AppendMediaPacket(buffered_media_packet.value());
		}
	}

	AppendMediaPacket(media_packet);
}

void LLHlsStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		_initial_media_packet_buffer.Enqueue(media_packet);
		return;
	}

	if (_initial_media_packet_buffer.IsEmpty() == false)
	{
		logtd("SendAudioFrame() - BufferSize (%u)", _initial_media_packet_buffer.Size());
		while (_initial_media_packet_buffer.IsEmpty() == false)
		{
			auto buffered_media_packet = _initial_media_packet_buffer.Dequeue();
			if (buffered_media_packet.has_value() == false)
			{
				continue;
			}

			AppendMediaPacket(buffered_media_packet.value());
		}
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
	return ov::String::FormatString("../%s/chunklist_%d_%s_llhls.m3u8",
										GetName().CStr(),
										track_id, 
										StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr());
}

ov::String LLHlsStream::GetIntializationSegmentName(const int32_t &track_id) const
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

ov::String LLHlsStream::GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number) const
{
	// part_<track id>_<segment number>_<partial number>_<media type>_<random str>_llhls.m4s
	return ov::String::FormatString("part_%d_%lld_%lld_%s_%s_llhls.m4s", 
									track_id,
									segment_number,
									partial_number + 1,
									StringFromMediaType(GetTrack(track_id)->GetMediaType()).LowerCaseString().CStr(),
									_stream_key.CStr());
}

void LLHlsStream::OnFMp4StorageInitialized(const int32_t &track_id)
{
	// milliseconds to seconds
	auto segment_duration = static_cast<float_t>(_storage_config.segment_duration_ms) / 1000.0;
	auto chunk_duration = static_cast<float_t>(_packager_config.chunk_duration_ms) / 1000.0;

	auto playlist = std::make_shared<LLHlsChunklist>(GetChunklistName(track_id),
													GetTrack(track_id),
													_storage_config.max_segments, 
													segment_duration, 
													chunk_duration, 
													GetIntializationSegmentName(track_id));

	std::unique_lock<std::shared_mutex> lock(_chunklist_map_lock);
	_chunklist_map[track_id] = playlist;
}

void LLHlsStream::CheckPlaylistReady()
{
	if (_playlist_ready == true)
	{
		return;
	}

	std::shared_lock<std::shared_mutex> storage_lock(_storage_map_lock);

	for (const auto &[track_id, storage] : _storage_map)
	{
		if (storage->GetLastSegmentNumber() < 0)
		{
			return;
		}

		_max_chunk_duration_ms = std::max(_max_chunk_duration_ms, storage->GetMaxChunkDurationMs());
		_min_chunk_duration_ms = std::min(_min_chunk_duration_ms, storage->GetMinChunkDurationMs());
	}

	storage_lock.unlock();

	std::shared_lock<std::shared_mutex> chunklist_lock(_chunklist_map_lock);
	
	float_t part_hold_back = (static_cast<float_t>(_max_chunk_duration_ms) / 1000.0f) * 3.0f;
	for (const auto &[track_id, chunklist] : _chunklist_map)
	{
		chunklist->SetPartHoldBack(part_hold_back);
	}

	_playlist_ready = true;
}

void LLHlsStream::OnMediaSegmentUpdated(const int32_t &track_id, const uint32_t &segment_number)
{
	if (_playlist_ready == false)
	{
		CheckPlaylistReady();
	}

	auto playlist = GetChunklistWriter(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto segment = GetStorage(track_id)->GetMediaSegment(segment_number);

	// Timescale to seconds(demical)
	auto segment_duration = static_cast<float>(segment->GetDuration()) / static_cast<float>(1000.0);

	auto start_timestamp_ms = (static_cast<float>(segment->GetStartTimestamp()) / GetTrack(track_id)->GetTimeBase().GetTimescale()) * 1000.0;
	auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(GetStartedTime().time_since_epoch()).count() + start_timestamp_ms;

	auto segment_info = LLHlsChunklist::SegmentInfo(segment->GetNumber(), start_timestamp, segment_duration,
													segment->GetSize(), GetSegmentName(track_id, segment->GetNumber()), "", true);

	playlist->AppendSegmentInfo(segment_info);

	logtd("Media segment updated : track_id = %d, segment_number = %d, start_timestamp = %llu, segment_duration = %f", track_id, segment_number, segment->GetStartTimestamp(), segment_duration);
}

void LLHlsStream::OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number)
{
	auto playlist = GetChunklistWriter(track_id);
	if (playlist == nullptr)
	{
		logte("Playlist is not found : track_id = %d", track_id);
		return;
	}

	auto chunk = GetStorage(track_id)->GetMediaChunk(segment_number, chunk_number);
	
	// Milliseconds
	auto chunk_duration = static_cast<float>(chunk->GetDuration()) / static_cast<float>(1000.0);

	// Human readable timestamp
	auto start_timestamp_ms = (static_cast<float>(chunk->GetStartTimestamp()) / GetTrack(track_id)->GetTimeBase().GetTimescale()) * 1000.0;
	auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(GetCreatedTime().time_since_epoch()).count() + start_timestamp_ms;

	auto chunk_info = LLHlsChunklist::SegmentInfo(chunk->GetNumber(), start_timestamp, chunk_duration, chunk->GetSize(), 
												GetPartialSegmentName(track_id, segment_number, chunk->GetNumber()), 
												GetNextPartialSegmentName(track_id, segment_number, chunk->GetNumber()), chunk->IsIndependent());

	playlist->AppendPartialSegmentInfo(segment_number, chunk_info);

	logtd("Media chunk updated : track_id = %d, segment_number = %d, chunk_number = %d, start_timestamp = %llu, chunk_duration = %f", track_id, segment_number, chunk_number, chunk->GetStartTimestamp(), chunk_duration);

	// Notify
	NotifyPlaylistUpdated(track_id, segment_number, chunk_number);
}

void LLHlsStream::NotifyPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part)
{

	// Make std::any for broadcast
	// I think make_shared is better than copy sizeof(PlaylistUpdatedEvent) to all sessions
	auto event = std::make_shared<PlaylistUpdatedEvent>(track_id, msn, part);
	auto notification = std::make_any<std::shared_ptr<PlaylistUpdatedEvent>>(event);
	BroadcastPacket(notification);
}