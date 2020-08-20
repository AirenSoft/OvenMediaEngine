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
#include <vector>
#include <queue>

#include "base/info/stream.h"
#include "base/mediarouter/media_route_application_connector.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"

enum class MRStreamInoutType : int8_t
{
	Unknown = -1,
	Incoming,
	Outgoing
};

class MediaRouteApplication;

class MediaRouteStream
{
public:
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream);
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MRStreamInoutType inout_type);
	~MediaRouteStream();

	// Inout Stream Type
	void SetInoutType(MRStreamInoutType inout_type);
	MRStreamInoutType GetInoutType();

	// Queue interfaces
	bool Push(std::shared_ptr<MediaPacket> media_packet);
	bool PushIncomingStream(std::shared_ptr<MediaPacket> &media_packet);
	bool PushOutgoungStream(std::shared_ptr<MediaPacket> &media_packet);

	std::shared_ptr<MediaPacket> Pop();

	// Query original stream information
	std::shared_ptr<info::Stream> GetStream();

	bool IsValidStream();
	bool IsCreatedSteam();
	void SetCreatedSteam(bool created);

private:
	
	// Whether to generate output streams corresponding to the current mr stream.
	bool _created_stream;

	std::shared_ptr<MediaRouteApplication> _parent;

	// Incoming/Outgoing Stream
	MRStreamInoutType _inout_type;
	
	// Connector Type
	MediaRouteApplicationConnector::ConnectorType _application_connector_type2;

	// Stream Information
	std::shared_ptr<info::Stream> _stream;

	// Temporary packet store. for calculating packet duration
	std::map<uint8_t, std::shared_ptr<MediaPacket>> _media_packet_stored;

	// Packets queue
	ov::Queue<std::shared_ptr<MediaPacket>> _packets_queue;

	// Store the correction values in case of sudden change in PTS.
	// If the PTS suddenly increases, the filter behaves incorrectly.
	// <TrackId, Pts>
	std::map<uint8_t, int64_t> _pts_correct;
	// Average Pts Incresement
	std::map<uint8_t, int64_t> _pts_avg_inc;

	// Time for statistics
	ov::StopWatch _stop_watch;
	std::chrono::time_point<std::chrono::system_clock> _last_recv_time;
	std::chrono::time_point<std::chrono::system_clock> _stat_start_time;

	// Statistics
	// <TrackId, Values>
	std::map<uint8_t, int64_t> _stat_recv_pkt_lpts;
	std::map<uint8_t, int64_t> _stat_recv_pkt_ldts;
	std::map<uint8_t, int64_t> _stat_recv_pkt_size;
	std::map<uint8_t, int64_t> _stat_recv_pkt_count;
	std::map<uint8_t, int64_t> _stat_first_time_diff;
};

