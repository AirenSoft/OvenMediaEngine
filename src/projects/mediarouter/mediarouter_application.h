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

#include "base/mediarouter/media_route_application_observer.h"
#include "base/mediarouter/media_route_application_connector.h"
#include "base/mediarouter/media_route_application_interface.h"
#include "base/mediarouter/media_route_interface.h"


#include "mediarouter_stream.h"

#include <config/items/items.h>

class ApplicationInfo;
class Stream;
class RelayServer;
class RelayClient;

class MediaRouteApplication : public MediaRouteApplicationInterface
{
public:
	static std::shared_ptr<MediaRouteApplication> Create(
		const info::Application &application_info);

	explicit MediaRouteApplication(
		const info::Application &application_info);
	
	~MediaRouteApplication() override;

public:
	bool Start();
	bool Stop();

	volatile bool _kill_flag;
	std::thread _thread;

public:
	// Register/unregister connector from provider
	bool RegisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	bool UnregisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	// Register/unregister connector from publisher
	bool RegisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);

	bool UnregisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);

public:

	bool OnCreateStream(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream) override;

	bool NotifyCreateStream(
		const std::shared_ptr<info::Stream> &stream_info, 
		MediaRouteApplicationConnector::ConnectorType connector_type);

	bool OnDeleteStream(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn, 
		const std::shared_ptr<info::Stream> &stream) override;

	bool NotifyDeleteStream(
		const std::shared_ptr<info::Stream> &stream_info, 
		const MediaRouteApplicationConnector::ConnectorType connector_type);

	bool OnReceiveBuffer(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream,
		const std::shared_ptr<MediaPacket> &packet) override;

private:
	bool ReuseIncomingStream(const std::shared_ptr<info::Stream> &stream_info);
	bool CreateIncomingStream(const std::shared_ptr<info::Stream> &stream_info);
	bool CreateOutgoingStream(const std::shared_ptr<info::Stream> &stream_info);

	bool DeleteIncomingStream(const std::shared_ptr<info::Stream> &stream_info);
	bool DeleteOutgoingStream(const std::shared_ptr<info::Stream> &stream_info);

	std::shared_ptr<MediaRouteStream> GetStream(uint8_t indicator, uint32_t stream_id);

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

	class BufferIndicator
	{
	public:
		enum BufferIndicatorEnum : uint8_t 
		{
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
