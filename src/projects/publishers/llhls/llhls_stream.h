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
#include <base/info/dump.h>
#include <modules/dump/dump.h>

#include "monitoring/monitoring.h"

#include "modules/containers/bmff/fmp4_packager/fmp4_packager.h"
#include "llhls_master_playlist.h"
#include "llhls_chunklist.h"

#define DEFAULT_PLAYLIST_NAME	"llhls.m3u8"


// max initial media packet buffer size, for OOM protection
#define MAX_INITIAL_MEDIA_PACKET_BUFFER_SIZE		10000

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
	void SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void OnEvent(const std::shared_ptr<MediaEvent> &event) override;

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

	uint64_t GetMaxChunkDurationMS() const;

	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetMasterPlaylist(const ov::String &file_name, const ov::String &chunk_query_string, bool gzip, bool legacy, bool rewind, bool include_path=true);
	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetChunklist(const ov::String &chunk_query_string, const int32_t &track_id, int64_t msn, int64_t psn, bool skip, bool gzip, bool legacy, bool rewind) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetInitializationSegment(const int32_t &track_id) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetSegment(const int32_t &track_id, const int64_t &segment_number) const;
	std::tuple<RequestResult, std::shared_ptr<ov::Data>> GetChunk(const int32_t &track_id, const int64_t &segment_number, const int64_t &chunk_number) const;

	//////////////////////////
	// For Dump API
	//////////////////////////

	// <result, error message>
	std::tuple<bool, ov::String> StartDump(const std::shared_ptr<info::Dump> &dump_info);
	std::tuple<bool, ov::String> StopDump(const std::shared_ptr<info::Dump> &dump_info);
	// Get dump info
	std::shared_ptr<const mdl::Dump> GetDumpInfo(const ov::String &dump_id);
	// Get dumps
	std::vector<std::shared_ptr<const mdl::Dump>> GetDumpInfoList();

private:
	bool Start() override;
	bool Stop() override;

	bool GetDrmInfo(const ov::String &file_path, bmff::CencProperty &cenc_property);

	bool IsSupportedCodec(cmn::MediaCodecId codec_id) const; 

	void NotifyPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part);

	// bmff::FMp4StorageObserver implementation
	void OnFMp4StorageInitialized(const int32_t &track_id) override;
	void OnMediaSegmentCreated(const int32_t &track_id, const uint32_t &segment_number) override;
	void OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number, bool last_chunk) override;
	void OnMediaSegmentDeleted(const int32_t &track_id, const uint32_t &segment_number) override;

	// Create and Get fMP4 packager and storage with track info, storage and packager_config
	bool AddPackager(const std::shared_ptr<const MediaTrack> &media_track, const std::shared_ptr<const MediaTrack> &data_track);

	// Get fMP4 packager with the track id
	std::shared_ptr<bmff::FMP4Packager> GetPackager(const int32_t &track_id) const;
	// Get storage with the track id
	std::shared_ptr<bmff::FMP4Storage> GetStorage(const int32_t &track_id) const;
	// Get Playlist with the track id
	std::shared_ptr<LLHlsChunklist> GetChunklistWriter(const int32_t &track_id) const;

	std::shared_ptr<LLHlsMasterPlaylist> CreateMasterPlaylist(const std::shared_ptr<const info::Playlist> &playlist) const;

	ov::String GetChunklistName(const int32_t &track_id) const;
	ov::String GetInitializationSegmentName(const int32_t &track_id) const;
	ov::String GetSegmentName(const int32_t &track_id, const int64_t &segment_number) const;
	ov::String GetPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number) const;
	ov::String GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number, bool last_chunk) const;

	bool AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);

	bool IsReadyToPlay() const;
	bool CheckPlaylistReady();

	void DumpMasterPlaylistsOfAllItems();
	bool DumpMasterPlaylist(const std::shared_ptr<mdl::Dump> &item);
	void DumpInitSegmentOfAllItems(const int32_t &track_id);
	bool DumpInitSegment(const std::shared_ptr<mdl::Dump> &item, const int32_t &track_id);
	void DumpSegmentOfAllItems(const int32_t &track_id, const uint32_t &segment_number);
	bool DumpSegment(const std::shared_ptr<mdl::Dump> &item, const int32_t &track_id, const int64_t &segment_number);

	bool DumpData(const std::shared_ptr<mdl::Dump> &item, const ov::String &file_name, const std::shared_ptr<const ov::Data> &data);

	int64_t GetMinimumLastSegmentNumber() const;
	bool StopToSaveOldSegmentsInfo();

	//////////////////////////
	// Events
	//////////////////////////

	// <result, error message>
	std::tuple<bool, ov::String> ConcludeLive();
	bool IsConcluded() const;

	// Config
	bmff::FMP4Packager::Config _packager_config;
	bmff::FMP4Storage::Config _storage_config;

	// Track ID : Storage
	std::map<int32_t, std::shared_ptr<bmff::FMP4Storage>> _storage_map;
	mutable std::shared_mutex _storage_map_lock;
	std::map<int32_t, std::shared_ptr<bmff::FMP4Packager>> _packager_map;
	mutable std::shared_mutex _packager_map_lock;
	std::map<int32_t, std::shared_ptr<LLHlsChunklist>> _chunklist_map;
	mutable std::shared_mutex _chunklist_map_lock;

	uint64_t _max_chunk_duration_ms = 0;
	uint64_t _min_chunk_duration_ms = std::numeric_limits<uint64_t>::max();

	double _configured_part_hold_back = 0;
	bool _preload_hint_enabled = true;

	std::map<ov::String, std::shared_ptr<LLHlsMasterPlaylist>> _master_playlists;
	std::mutex _master_playlists_lock;

	bool _playlist_ready = false;
	mutable std::shared_mutex _playlist_ready_lock;

	// Reserve
	void BufferMediaPacketUntilReadyToPlay(const std::shared_ptr<MediaPacket> &media_packet);
	bool SendBufferedPackets();
	ov::Queue<std::shared_ptr<MediaPacket>> _initial_media_packet_buffer;

	bool _origin_mode = false;
	ov::String _stream_key;

	uint32_t _worker_count = 0;

	std::map<ov::String, std::shared_ptr<mdl::Dump>> _dumps;
	std::shared_mutex _dumps_lock;

	// DRM
	bool _indentity_enabled = false; // for custom license server and player purposes
	bool _widevine_enabled = false;
	bool _playready_enabled = false;
	bool _fairplay_enabled = false;

	bmff::CencProperty _cenc_property;
	ov::String _key_uri; // string, only for FairPlay

	// PROGRAM-DATE-TIME
	bool _first_chunk = true;
	int64_t _wallclock_offset_ms = 0;

	// ConcludeLive
	// Append #EXT-X-ENDLIST all chunklists, and no more update segment and chunklist
	bool _concluded = false;
	mutable std::shared_mutex _concluded_lock;
};