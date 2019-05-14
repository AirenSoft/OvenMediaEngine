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
#include "base/media_route/media_queue.h"

#include "media_route_stream.h"

#include <config/items/application.h>

class ApplicationInfo;
class StreamInfo;

class RelayServer;
class RelayClient;

// -어플리케이션(Application) 별 스트림(Stream)을 관리해야 한다
// - Publisher를 관리해야한다
// - Provider를 관리해야한다

// 1. 어플리케이션 pool 을 미리 생성한다.

// 1. Connect App에 접속되면 어플리케이션으로 할당함.

// Stream timout for GarbageCollector
# define TIMEOUT_STREAM_ALIVE   30

class MediaRouteApplication : public MediaRouteApplicationInterface
{
public:
	static std::shared_ptr<MediaRouteApplication> Create(const info::Application *application_info);

	explicit MediaRouteApplication(const info::Application *application_info);
	~MediaRouteApplication() override;

public:
	bool Start();
	bool Stop();

	volatile bool _kill_flag;
	std::thread _thread;
	std::mutex _mutex;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 프로바이더(Provider) 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 본 데이터 모듈에 넣어주는 놈들
public:
	bool RegisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	bool UnregisterConnectorApp(
		std::shared_ptr<MediaRouteApplicationConnector> connector);

	// 스트림 생성
	bool OnCreateStream(
		std::shared_ptr<MediaRouteApplicationConnector> app_conn,
		std::shared_ptr<StreamInfo> stream) override;

	// 스트림 삭제
	bool OnDeleteStream(
		std::shared_ptr<MediaRouteApplicationConnector> app_conn,
		std::shared_ptr<StreamInfo> stream) override;

	// 미디어 버퍼 수신
	bool OnReceiveBuffer(
		std::shared_ptr<MediaRouteApplicationConnector> app_conn,
		std::shared_ptr<StreamInfo> stream,
		std::unique_ptr<MediaPacket> packet) override;


public:

	bool RegisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);

	bool UnregisterObserverApp(
		std::shared_ptr<MediaRouteApplicationObserver> observer);


public:
	// Application information from configuration file
	const info::Application *_application_info;

	// Information of Connector instance
	std::vector<std::shared_ptr<MediaRouteApplicationConnector>> _connectors;

	// Information of Observer instance
	std::vector<std::shared_ptr<MediaRouteApplicationObserver>> _observers;

	// Information of MediaStream instance
	// Key : StreamInfo.id
	std::map<uint32_t, std::shared_ptr<MediaRouteStream>> _streams;

public:
	void MainTask();

	void OnGarbageCollector();
	void GarbageCollector();

	const std::map<uint32_t, std::shared_ptr<MediaRouteStream>> GetStreams() const override
	{
		return _streams;
	}

	std::shared_ptr<RelayClient> GetOriginConnector() override
	{
		return _relay_client;
	}

	enum
	{
		BUFFFER_INDICATOR_UNIQUEID_GC = 0
	};

	class BufferIndicator
	{
	public:
		explicit BufferIndicator(uint32_t stream_id)
		{
			_stream_id = stream_id;
		}

		uint32_t _stream_id;
	};


protected:
	// 버퍼를 처리할 인디게이터
	MediaQueue<std::unique_ptr<BufferIndicator>> _indicator;

	std::shared_ptr<RelayServer>    _relay_server;
	std::shared_ptr<RelayClient>    _relay_client;
	ov::DelayQueue                  _retry_timer;
};
