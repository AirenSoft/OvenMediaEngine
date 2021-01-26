//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/items/items.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/mediarouter/media_route_application_connector.h"
#include "base/mediarouter/media_route_application_interface.h"
#include "base/mediarouter/media_route_application_observer.h"
#include "base/mediarouter/media_route_interface.h"
#include "mediarouter_stream.h"

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


public:
	//////////////////////////////////////////////////////////////////////
	// Interface for Application Binding
	//////////////////////////////////////////////////////////////////////

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
	//////////////////////////////////////////////////////////////////////
	// Interface for Stream and MediaPacket
	//////////////////////////////////////////////////////////////////////
	bool OnStreamCreated(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream) override;

	bool OnStreamDeleted(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream) override;

	bool OnPacketReceived(
		const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
		const std::shared_ptr<info::Stream> &stream,
		const std::shared_ptr<MediaPacket> &packet) override;

	bool IsExistingInboundStream(ov::String stream_name) override;


public:
	bool NotifyStreamCreate(
		const std::shared_ptr<info::Stream> &stream_info,
		MediaRouteApplicationConnector::ConnectorType connector_type);

	bool NotifyStreamPrepared(std::shared_ptr<MediaRouteStream> &stream);
	
	bool NotifyStreamDelete(
		const std::shared_ptr<info::Stream> &stream_info,
		const MediaRouteApplicationConnector::ConnectorType connector_type);

private:
	std::shared_ptr<MediaRouteStream> CreateInboundStream(const std::shared_ptr<info::Stream> &stream_info);
	std::shared_ptr<MediaRouteStream> CreateOutboundStream(const std::shared_ptr<info::Stream> &stream_info);

	bool DeleteInboundStream(const std::shared_ptr<info::Stream> &stream_info);
	bool DeleteOutboundStream(const std::shared_ptr<info::Stream> &stream_info);

	// std::shared_ptr<MediaRouteStream> GetStream(uint8_t indicator, uint32_t stream_id);
	std::shared_ptr<MediaRouteStream> GetInboundStream(uint32_t stream_id);
	std::shared_ptr<MediaRouteStream> GetInboundStreamByName(const ov::String stream_name);
	std::shared_ptr<MediaRouteStream> GetOutboundStream(uint32_t stream_id);
	std::shared_ptr<MediaRouteStream> GetOutboundStreamByName(const ov::String stream_name);

private:
	// Application information from configuration file
	const info::Application _application_info;

	// Information of Connector instance
	std::vector<std::shared_ptr<MediaRouteApplicationConnector>> _connectors;
	std::shared_mutex _connectors_lock;

	// Information of Observer instance
	std::vector<std::shared_ptr<MediaRouteApplicationObserver>> _observers;
	std::shared_mutex _observers_lock;

	// Information of MediaStream instance
	// Inbound Streams
	// Key : Stream.id
	std::map<uint32_t, std::shared_ptr<MediaRouteStream>> _inbound_streams;

	// Outbound Streams
	// Key : Stream.id
	std::map<uint32_t, std::shared_ptr<MediaRouteStream>> _outbound_streams;
	std::shared_mutex _streams_lock;

private:
	void InboundWorkerThread(uint32_t worker_id);
	void OutboundWorkerThread(uint32_t worker_id);

	volatile bool _kill_flag;
	std::vector<std::thread> _inbound_threads;
	std::vector<std::thread> _outbound_threads;

	uint32_t _max_worker_thread_count;

private:
	std::vector<std::shared_ptr<ov::Queue<std::shared_ptr<MediaRouteStream>>>> _inbound_stream_indicator;
	std::vector<std::shared_ptr<ov::Queue<std::shared_ptr<MediaRouteStream>>>> _outbound_stream_indicator;
};
