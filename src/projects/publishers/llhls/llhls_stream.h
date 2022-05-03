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
#include "llhls_playlist.h"

class LLHlsStream : public pub::Stream, public bmff::FMp4StorageObserver
{
public:
	static std::shared_ptr<LLHlsStream> Create(const std::shared_ptr<pub::Application> application, const info::Stream &info);

	explicit LLHlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info);
	~LLHlsStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

private:
	bool Start() override;
	bool Stop() override;

	// bmff::FMp4StorageObserver implementation
	void OnFMp4StorageInitialized(const int32_t &track_id) override;
	void OnMediaSegmentUpdated(const int32_t &track_id, const uint32_t &segment_number) override;
	void OnMediaChunkUpdated(const int32_t &track_id, const uint32_t &segment_number, const uint32_t &chunk_number) override;

	// Create and Get fMP4 packager and storage with track info, storage and packager_config
	bool AddPackager(const std::shared_ptr<const MediaTrack> &track);
	// Get fMP4 packager with the track id
	std::shared_ptr<bmff::FMP4Packager> GetPackager(const int32_t &track_id);
	// Get storage with the track id
	std::shared_ptr<bmff::FMP4Storage> GetStorage(const int32_t &track_id);
	// Get Playlist with the track id
	std::shared_ptr<LLHlsPlaylist> GetPlaylist(const int32_t &track_id);

	ov::String GetIntializationSegmentName(const int32_t &track_id);
	ov::String GetSegmentName(const int32_t &track_id, const int64_t &segment_number);
	ov::String GetPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);
	ov::String GetNextPartialSegmentName(const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);

	enum class FileType : uint8_t
	{
		InitializationSegment,
		Segment,
		PartialSegment,
	};

	bool ParseFileName(const ov::String &file_name, FileType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number);

	bool AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);

	// Config
	bmff::FMP4Packager::Config _packager_config;
	bmff::FMP4Storage::Config _storage_config;

	// Track ID : Stroage
	std::map<int32_t, std::shared_ptr<bmff::FMP4Storage>> _storage_map;
	std::shared_mutex _storage_map_lock;
	std::map<int32_t, std::shared_ptr<bmff::FMP4Packager>> _packager_map;
	std::shared_mutex _packager_map_lock;
	std::map<int32_t, std::shared_ptr<LLHlsPlaylist>> _playlist_map;
	std::shared_mutex _playlist_map_lock;
};
