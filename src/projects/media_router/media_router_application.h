//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <algorithm>

#include "base/media_route/media_route_application_observer.h"
#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_route_interface.h"
#include "base/media_route/media_route_application_interface.h"

#include "media_router_stream.h"

#include <config/items/items.h>

class ApplicationInfo;
class Stream;

class RelayServer;
class RelayClient;

class MediaRouteApplication : public MediaRouteApplicationInterface
{
public:
	static std::shared_ptr<MediaRouteApplication> Create(const info::Application &application_info);

	explicit MediaRouteApplication(const info::Application &application_info);
	~MediaRouteApplication() override;

public:
	bool Start();
	bool Stop();

	volatile bool _kill_flag;
	std::thread _thread;

public:
	bool RegisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	bool UnregisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	// 스트림 생성
	bool OnCreateStream(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream) override;

	// 스트림 삭제
	bool OnDeleteStream(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream) override;

	// 미디어 버퍼 수신
	bool OnReceiveBuffer(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream,
		const std::shared_ptr<MediaPacket> &packet) override;

public:
	bool RegisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);

	bool UnregisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);

public:
	// Application information from configuration file
	const info::Application _application_info;

	// Information of Connector instance
	std::vector<std::shared_ptr<MediaRouteApplicationConnector>> _connectors;
	std::shared_mutex _connectors_lock;

	// Information of Observer instance
	std::vector<std::shared_ptr<MediaRouteApplicationObserver>> _observers;
	std::shared_mutex _observers_lock;

	// Information of MediaStream instance
	// Incoming Streams
	// Key : Stream.id
	std::map<uint32_t, std::shared_ptr<MediaRouteStream>> _streams_incoming;

	// Outgoing Streams
	// Key : Stream.id
	std::map<uint32_t, std::shared_ptr<MediaRouteStream>> _streams_outgoing;
	
	std::shared_mutex _streams_lock;

public:
	void MessageLooper();

	// enum StreamBufferIndicator {
	// 	BUFFER_INDICATOR_NONE_STREAM = 0,
	// 	BUFFER_INDICATOR_INCOMING_STREAM,
	// 	BUFFER_INDICATOR_OUTGOING_STREAM
	// };

	class BufferIndicator
	{
	public:
		enum {
			BUFFER_INDICATOR_NONE_STREAM = 0,
			BUFFER_INDICATOR_INCOMING_STREAM,
			BUFFER_INDICATOR_OUTGOING_STREAM
		};

		explicit BufferIndicator(uint8_t inout, uint32_t stream_id)
		{
			_inout = inout;
			_stream_id = stream_id;
		}

		uint8_t _inout;
		uint32_t _stream_id;
	};


protected:
	ov::Queue<std::shared_ptr<BufferIndicator>> _indicator;
};
