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
#include "base/application/stream_info.h"

class MediaRouteStream
{
	friend class MediaRouteApplication;
public:
	MediaRouteStream(std::shared_ptr<StreamInfo> stream_info);
	~MediaRouteStream();

public:
	// 우너본 스트림 정보 조회
	std::shared_ptr<StreamInfo> GetStreamInfo();

	void SetConnectorType(MediaRouteApplicationConnector::ConnectorType type);
	MediaRouteApplicationConnector::ConnectorType GetConnectorType();

private:
	std::shared_ptr<StreamInfo> _stream_info;

	MediaRouteApplicationConnector::ConnectorType _application_connector_type;

public:
	// 패킷 관리
	bool Push(std::unique_ptr<MediaPacket> buffer);
	std::unique_ptr<MediaPacket> Pop();
	uint32_t Size();

	time_t getLastReceivedTime();
private:
	std::queue<std::unique_ptr<MediaPacket>> _queue;

private:
	// 마지막으로 받은 패킷의 시간
	time_t _last_rb_time;
};

