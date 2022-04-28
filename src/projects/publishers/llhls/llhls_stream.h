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

class LLHlsStream : public pub::Stream
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

	// Create and Get fMP4 packager and storage with track info, storage and packager_config
	bool AddPackager(const std::shared_ptr<const MediaTrack> &track, const bmff::FMP4Packager::Config &packager_config, const bmff::FMP4Storage::Config &storage_config);
	// Get fMP4 packager with the track id
	std::shared_ptr<bmff::FMP4Packager> GetPackager(const int32_t &track_id);
	// Get storage with the track id
	std::shared_ptr<bmff::FMP4Storage> GetStorage(const int32_t &track_id);

	bool AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);

	// Track ID : Stroage
	std::map<int32_t, std::shared_ptr<bmff::FMP4Storage>> _storage_map;
	std::shared_mutex _storage_map_lock;
	std::map<int32_t, std::shared_ptr<bmff::FMP4Packager>> _packager_map;
	std::shared_mutex _packager_map_lock;
};
