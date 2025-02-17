//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
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
#include "base/mediarouter/media_type.h"
#include "mediarouter_nomalize.h"
#include "mediarouter_stats.h"
#include "mediarouter_event_generator.h"
#include "modules/managed_queue/managed_queue.h"

enum class MediaRouterStreamType : int8_t
{
	UNKNOWN = -1,
	INBOUND,
	OUTBOUND
};

class MediaRouteStream : public MediaRouterNormalize, public MediaRouterStats, public MediaRouterEventGenerator
{
public:
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MediaRouterStreamType type);
	~MediaRouteStream();

	// Inout Stream Type
	void SetType(MediaRouterStreamType type);
	bool IsInbound() { return _type == MediaRouterStreamType::INBOUND; }
	bool IsOutbound() { return _type == MediaRouterStreamType::OUTBOUND; }

	// Queue interfaces
	void Push(const std::shared_ptr<MediaPacket> &media_packet);
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
	bool IsStreamReady();

	void Flush();
	
private:
	void DropNonDecodingPackets();
	void DetectAbnormalPackets(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &packet);

	bool _is_stream_prepared = false;
	bool _is_all_tracks_parsed = false;

	// Incoming/Outgoing Stream
	MediaRouterStreamType _type;

	// Stream Information
	std::shared_ptr<info::Stream> _stream = nullptr;

	// Temporary packet store. for calculating packet duration
	std::map<MediaTrackId, std::shared_ptr<MediaPacket>> _media_packet_stash;

	// Packets queue
	ov::ManagedQueue<std::shared_ptr<MediaPacket>> _packets_queue;

	// Mirror buffer
	std::vector<std::shared_ptr<MirrorBufferItem>> _mirror_buffer;

	// Store the correction values in case of sudden change in PTS.
	// If the PTS suddenly increases, the filter behaves incorrectly.
	std::map<MediaTrackId, int64_t> _pts_last;
};
