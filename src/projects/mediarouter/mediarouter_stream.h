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

typedef int32_t MediaTrackId;

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
	bool PushIncomingStream(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet);
	bool PushOutgoungStream(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet);

	std::shared_ptr<MediaPacket> Pop();

	// Query original stream information
	std::shared_ptr<info::Stream> GetStream();

	bool IsCreatedSteam();
	void SetCreatedSteam(bool created);

	bool IsParseTrackAll();

private:
	void InitParseTrackInfo();
	void SetParseTrackInfo(std::shared_ptr<MediaTrack> &media_track, bool parsed);
	bool GetParseTrackInfo(std::shared_ptr<MediaTrack> &media_track);

	// Parse media track information
	bool ParseTrackInfo(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet);

	// Set whether parsing media track is complete
	std::map<MediaTrackId, bool> _parse_completed_track_info;
	bool _is_parsed_all_track;

private:
	void DropNonDecodingPackets();

private:
	// Convert to default bitstream format
	bool ConvertToDefaultBitstream(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet);

	// Parse fragment header, flags
	bool UpdateFlagmentHeaders(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet, bool force = false);

	// Periodically insert sps/pps so that the player's decoding starts quickly.
	bool UpdateDecoderParameterSets(
		std::shared_ptr<MediaTrack> &media_track,
		std::shared_ptr<MediaPacket> &media_packet);


	bool UpdateKeyFlags(
		std::shared_ptr<MediaTrack> &media_track, 
		std::shared_ptr<MediaPacket> &media_packet);

	void UpdateStatistics(std::shared_ptr<MediaTrack> &media_track,
		std::shared_ptr<MediaPacket> &media_packet);
private:
	
	// Whether to generate output streams corresponding to the current mr stream.
	bool _is_created_stream;

	// Incoming/Outgoing Stream
	MRStreamInoutType _inout_type;
	
	// Connector Type
	MediaRouteApplicationConnector::ConnectorType _application_connector_type;

	// Stream Information
	std::shared_ptr<info::Stream> _stream;

	// Temporary packet store. for calculating packet duration
	std::map<MediaTrackId, std::shared_ptr<MediaPacket>> _media_packet_stash;

	// Packets queue
	ov::Queue<std::shared_ptr<MediaPacket>> _packets_queue;

	// Store the correction values in case of sudden change in PTS.
	// If the PTS suddenly increases, the filter behaves incorrectly.
	std::map<MediaTrackId, int64_t> _pts_last;
	std::map<MediaTrackId, int64_t> _dts_last;
	// <TrackId, Pts>
	std::map<MediaTrackId, int64_t> _pts_correct;
	// Average Pts Incresement
	std::map<MediaTrackId, int64_t> _pts_avg_inc;

	// Timebase of incoming packets
	std::map<MediaTrackId, common::Timebase> _incoming_tiembase;

	// Statistics
	// <TrackId, Values>
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_lpts;
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_ldts;
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_size;
	std::map<MediaTrackId, int64_t> _stat_recv_pkt_count;
	std::map<MediaTrackId, int64_t> _stat_first_time_diff;

	// Time for statistics
	ov::StopWatch _stop_watch;
	std::chrono::time_point<std::chrono::system_clock> _last_recv_time;
	std::chrono::time_point<std::chrono::system_clock> _stat_start_time;

private:
	void DumpPacket(
		std::shared_ptr<MediaPacket> &media_packet,
		bool dump = false);
};

