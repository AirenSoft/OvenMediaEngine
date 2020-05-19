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

#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "base/info/stream.h"

#include "bitstream/bitstream_to_annexb.h"
#include "bitstream/bitstream_to_adts.h"
#include "bitstream/bitstream_to_annexa.h"

#include "bitstream/avc_video_packet_fragmentizer.h"

class MediaRouteStream
{
public:
	MediaRouteStream(const std::shared_ptr<info::Stream> &stream);
	~MediaRouteStream();

	void SetInoutType(bool inout_type);

	// Query original stream information
	std::shared_ptr<info::Stream> GetStream();
	void SetConnectorType(MediaRouteApplicationConnector::ConnectorType type);
	MediaRouteApplicationConnector::ConnectorType GetConnectorType();

	// Queue interfaces
	bool Push(std::shared_ptr<MediaPacket> media_packet);
	std::shared_ptr<MediaPacket> Pop();

private:
	// false = incoming stream
	// true = outgoing stream
	bool	_inout_type;

	std::shared_ptr<info::Stream> _stream;
	MediaRouteApplicationConnector::ConnectorType _application_connector_type;

	std::map<uint8_t, std::shared_ptr<MediaPacket>> _media_packet_stored;
	ov::Queue<std::shared_ptr<MediaPacket>> _media_packets;

	////////////////////////////
	// bitstream filters
	////////////////////////////
	BitstreamToAnnexB _bsfv;
	BitstreamToADTS _bsfa;
	BitstreamAnnexA _bsf_vp8;

	AvcVideoPacketFragmentizer _avc_video_fragmentizer;

	// Store the correction values in case of sudden change in PTS.
	// If the PTS suddenly increases, the filter behaves incorrectly.
	std::map<uint8_t, int64_t> _pts_correct;
	// Average Pts Incresement
	std::map<uint8_t, int64_t> _pts_avg_inc;

	// statistics 
	ov::StopWatch _stop_watch;

	std::chrono::time_point<std::chrono::system_clock> _last_recv_time;
	std::chrono::time_point<std::chrono::system_clock> _stat_start_time;

	std::map<uint8_t, int64_t> _stat_recv_pkt_lpts;
	std::map<uint8_t, int64_t> _stat_recv_pkt_ldts;
	std::map<uint8_t, int64_t> _stat_recv_pkt_size;
	std::map<uint8_t, int64_t> _stat_recv_pkt_count;

	std::map<uint8_t, int64_t> _stat_first_time_diff;
};

