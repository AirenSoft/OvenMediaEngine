//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include "monitoring/monitoring.h"

#include "modules/containers/bmff/fmp4_packager/fmp4_packager.h"
#include "llhls_master_playlist.h"
#include "llhls_chunklist.h"

class LLHlsStream : public pub::Stream, public bmff::FMp4StorageObserver
{
public:
	static std::shared_ptr<LLHlsStream> Create(const std::shared_ptr<pub::Application> application, 
												const info::Stream &info,
												uint32_t worker_count);

	explicit LLHlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count);
	~LLHlsStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	enum class RequestResult : uint8_t
	{
		Success, // Success
		Accepted, // The request is accepted but not yet processed, it will be processed later
		NotFound, // The request is not found
		UnknownError,
	};

	struct PlaylistUpdatedEvent
	{
		PlaylistUpdatedEvent(const int32_t &id, const int64_t &new_msn, const int64_t &new_part)
		{
			track_id = id;
			msn = new_msn;
			part = new_part;
		}

		int32_t track_id;
		int64_t msn;
		int64_t part;
	};
	
	const ov::String &GetStreamKey() const;

	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetPlaylist(const ov::String &chunk_query_string, bool gzip=false) const;
	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetChunklist(const ov::String &chunk_query_string, const int32_t &track_id, int64_t msn, int64_t psn, bool skip = false, bool gzip=false) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetInitializationSegment(const int32_t &track_id) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetSegment(const int32_t &track_id, const int64_t &segment_number) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetChunk(const int32_t &track_id, const int64_t &segment_number, const int64_t &chunk_number) const;
	
private:
	bool Start() override;
	bool Stop() override;

	void NotifyPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part);

	// bmff::FMp4StorageObserver implementation
	void OnFMp4StorageInitialized(const int32_t &track_id) override;
	void OnMediaSegmentUpdated(const int32_t &track_id, const uint32_t &segment_number) override;
	void OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number) override;

	// Create and Get fMP4 packager and storage with track info, storage and packager_config
	bool AddPackager(const std::shared_ptr<const MediaTrack> &track);
	// Get fMP4 packager with the track id
	std::shared_ptr<bmff::FMP4Packager> GetPackager(const int32_t &track_id) const;
	// Get storage with the track id
	std::shared_ptr<bmff::FMP4Storage> GetStorage(const int32_t &track_id) const;
	// Get Playlist with the track id
	std::shared_ptr<LLHlsChunklist> GetChunklistWriter(const int32_t &track_id) const;

	// Add X-MEDIA to the master playlist
	void AddMediaCandidateToMasterPlaylist(const std::shared_ptr<const MediaTrack> &track);
	// Add X-STREAM-INF to the master playlist
	void AddStreamInfToMasterPlaylist(const std::shared_ptr<const MediaTrack> &video_track, const std::shared_ptr<const MediaTrack> &audio_track);

	ov::String GetPlaylistName();
	ov::String GetChunklistName(const int32_t &track_id);
	ov::String GetIntializationSegmentName(const int32_t &track_id);
	ov::String GetSegmentName(const int32_t &track_id, const int64_t &segment_number);
	ov::String GetPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);
	ov::String GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);

	bool AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);

	void CheckPlaylistReady();

	// Config
	bmff::FMP4Packager::Config _packager_config;
	bmff::FMP4Storage::Config _storage_config;

	// Track ID : Stroage
	std::map<int32_t, std::shared_ptr<bmff::FMP4Storage>> _storage_map;
	mutable std::shared_mutex _storage_map_lock;
	std::map<int32_t, std::shared_ptr<bmff::FMP4Packager>> _packager_map;
	mutable std::shared_mutex _packager_map_lock;
	std::map<int32_t, std::shared_ptr<LLHlsChunklist>> _chunklist_map;
	mutable std::shared_mutex _chunklist_map_lock;

	uint64_t _max_chunk_duration_ms = 0;
	uint64_t _min_chunk_duration_ms = std::numeric_limits<uint64_t>::max();

	LLHlsMasterPlaylist _master_playlist;
	bool _playlist_ready = false;

	ov::Queue<std::shared_ptr<MediaPacket>> _initial_media_packet_buffer;

	ov::String _stream_key;

	uint32_t _worker_count = 0;
};
