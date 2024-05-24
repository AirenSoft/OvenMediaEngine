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
#include <modules/containers/mpegts/mpegts_packetizer.h>
#include <modules/containers/mpegts/mpegts_packager.h>
#include <monitoring/monitoring.h>

#include "hls_master_playlist.h"
#include "hls_media_playlist.h"

#define TS_HLS_DEFAULT_PLAYLIST_NAME	"playlist.m3u8"

// max initial media packet buffer size, for OOM protection
#define MAX_INITIAL_MEDIA_PACKET_BUFFER_SIZE 10000

class HlsStream : public pub::Stream, public mpegts::PackagerSink
{
public:
	static std::shared_ptr<HlsStream> Create(const std::shared_ptr<pub::Application> application, 
												const info::Stream &info,
												uint32_t worker_count);

	explicit HlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info, uint32_t worker_count);
	~HlsStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	enum class RequestResult : uint8_t
	{
		Success, // Success
		NotFound, // The request is not found
		UnknownError,
	};

	// Implement mpegts::PackagerSink
	void OnSegmentCreated(const ov::String &packager_id, const std::shared_ptr<mpegts::Segment> &segment) override;
	void OnSegmentDeleted(const ov::String &packager_id, const std::shared_ptr<mpegts::Segment> &segment) override;

	// Interface for HLS Session
	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetMasterPlaylistData(const ov::String &playlist_name, bool rewind);
	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetMediaPlaylistData(const ov::String &variant_name, bool rewind);
	std::tuple<RequestResult, std::shared_ptr<const ov::Data>> GetSegmentData(const ov::String &variant_name, uint32_t number);

private:
	bool Start() override;
	bool Stop() override;
	bool IsSupportedCodec(cmn::MediaCodecId codec_id) const; 

	bool CreateDefaultPlaylist();
	bool CreatePackagers();

	ov::String GetVariantName(const ov::String &video_variant_name, const ov::String &audio_variant_name) const;
	ov::String GetMediaPlaylistName(const ov::String &variant_name) const;
	ov::String GetSegmentName(const ov::String &variant_name, uint32_t number) const;

	std::shared_ptr<mpegts::Packetizer> GetPacketizer(const ov::String &variant_name);
	std::shared_ptr<mpegts::Packager> GetPackager(const ov::String &variant_name);
	std::shared_ptr<HlsMediaPlaylist> GetMediaPlaylist(const ov::String &variant_name);
	std::shared_ptr<HlsMasterPlaylist> GetMasterPlaylist(const ov::String &playlist_name);

	uint32_t _worker_count = 0;

	cfg::vhost::app::pub::HlsPublisher _ts_config;
	// default querystring value
	bool _default_option_rewind = true;

	// packetizer id : Packetizer
	std::map<ov::String, std::shared_ptr<mpegts::Packetizer>> _packetizers;
	// All packetizers for each track
	std::map<uint32_t, std::vector<std::shared_ptr<mpegts::Packetizer>>> _track_packetizers;
	std::shared_mutex _packetizers_guard;

	std::map<ov::String, std::shared_ptr<mpegts::Packager>> _packagers;
	std::shared_mutex _packagers_guard;

	// playlist name : Master playlist
	std::map<ov::String, std::shared_ptr<HlsMasterPlaylist>> _master_playlists;
	std::shared_mutex _master_playlists_guard;

	// variant name : Media playlist
	std::map<ov::String, std::shared_ptr<HlsMediaPlaylist>> _media_playlists;
	std::shared_mutex _media_playlists_guard;
};