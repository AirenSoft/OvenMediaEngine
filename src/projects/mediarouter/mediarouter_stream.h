//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/mediarouter_application_connector.h"
#include "base/mediarouter/media_type.h"
#include "modules/managed_queue/managed_queue.h"

enum class MediaRouterStreamType : int8_t
{
	UNKNOWN = -1,
	INBOUND,
	OUTBOUND
};


class MediaRouteStream
{
public:
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream);
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MediaRouterStreamType inout_type);
	~MediaRouteStream();

	// Inout Stream Type
	void SetInoutType(MediaRouterStreamType inout_type);
	MediaRouterStreamType GetInoutType();

	// Queue interfaces
	void Push(const std::shared_ptr<MediaPacket> &media_packet);
	bool NormalizeMediaPacket(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);

	std::shared_ptr<MediaPacket> PopAndNormalize();
	
	// Return mirror buffer reference
	struct MirrorBufferItem
	{
		MirrorBufferItem(std::shared_ptr<MediaPacket> &packet)
			: packet(packet)
		{
			created_time = std::chrono::system_clock::now();
		}

		// Elapsed milliseconds
		int64_t GetElapsedMilliseconds()
		{
			auto now = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - created_time);
			return elapsed.count();
		}

		// timepoint created
		std::chrono::time_point<std::chrono::system_clock> created_time;
		std::shared_ptr<MediaPacket> packet;
	};
	std::vector<std::shared_ptr<MirrorBufferItem>> GetMirrorBuffer();

	// Query original stream information
	std::shared_ptr<info::Stream> GetStream();

	void OnStreamPrepared(bool completed);
	bool IsStreamPrepared();
	bool AreAllTracksReady();

	void Flush();
	
private:
	void DropNonDecodingPackets();
	void DetectAbnormalPackets(std::shared_ptr<MediaPacket> &packet);

	bool ProcessH264AVCCStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessH264AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool InsertH264SPSPPSAnnexB(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet, bool need_aud = false);
	
	bool InsertH264AudAnnexB(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessH265AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessH265HVCCStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessAACRawStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessAACAdtsStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessVP8Stream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessOPUSStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);
	bool ProcessMP3Stream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet);

	void UpdateStatistics(std::shared_ptr<MediaTrack> &media_track,  std::shared_ptr<MediaPacket> &media_packet);

	bool _is_stream_prepared = false;
	bool _are_all_tracks_parsed = false;

	// Incoming/Outgoing Stream
	MediaRouterStreamType _inout_type;

	// Connector Type
	MediaRouterApplicationConnector::ConnectorType _application_connector_type;

	// Stream Information
	std::shared_ptr<info::Stream> _stream = nullptr;

	// Temporary packet store. for calculating packet duration
	std::map<MediaTrackId, std::shared_ptr<MediaPacket>> _media_packet_stash;

	// Packets queue
	ov::ManagedQueue<std::shared_ptr<MediaPacket>> _packets_queue;

	std::vector<std::shared_ptr<MirrorBufferItem>> _mirror_buffer;

	// TODO(Soulk) : Modified to use by tying statistical information into a class and creating a map with MediaTrackId as a key

	// Store the correction values in case of sudden change in PTS.
	// If the PTS suddenly increases, the filter behaves incorrectly.
	std::map<MediaTrackId, int64_t> _pts_last;

	// <TrackId, Pts>
	std::map<MediaTrackId, int64_t> _pts_correct;
	// Average Pts Increment
	std::map<MediaTrackId, int64_t> _pts_avg_inc;

	// Statistics
	// <TrackId, Values>
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_lpts;
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_ldts;

	// Time for statistics
	ov::StopWatch _stop_watch;
	std::chrono::time_point<std::chrono::system_clock> _last_recv_time;
	std::chrono::time_point<std::chrono::system_clock> _stat_start_time;


	uint32_t _warning_count_bframe;
	uint32_t _warning_count_out_of_order;


	void DumpPacket(std::shared_ptr<MediaPacket> &media_packet, bool dump = false);
};
