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
#include "base/info/stream_info.h"

#include "bitstream/bitstream_to_annexb.h"
#include "bitstream/bitstream_to_adts.h"
#include "bitstream/bitstream_to_annexa.h"

#include "bitstream/avc_video_packet_fragmentizer.h"

class MediaRouteStream
{
public:
	MediaRouteStream(std::shared_ptr<StreamInfo> &stream_info);
	~MediaRouteStream();

	// Query original stream information
	std::shared_ptr<StreamInfo> GetStreamInfo();
	void SetConnectorType(MediaRouteApplicationConnector::ConnectorType type);
	MediaRouteApplicationConnector::ConnectorType GetConnectorType();

	// Queue interfaces
	bool Push(std::shared_ptr<MediaPacket> media_packet);
	std::shared_ptr<MediaPacket> Pop();
	uint32_t Size();

	time_t getLastReceivedTime();
private:
	std::shared_ptr<StreamInfo> _stream_info;
	MediaRouteApplicationConnector::ConnectorType _application_connector_type;


	// 2019/11/22 Getroot
	// Change shared_ptr to shared_ptr
	std::queue<std::shared_ptr<MediaPacket>> _media_packets;

	////////////////////////////
	// bitstream filters
	////////////////////////////
	BitstreamToAnnexB _bsfv;
	BitstreamToADTS _bsfa;
	BitstreamAnnexA _bsf_vp8;

	AvcVideoPacketFragmentizer _avc_video_packet_fragmentizer;
	
	int64_t _last_video_pts;
	int64_t _last_audio_pts;
	// time of last packet received
	time_t _last_rb_time;
};

