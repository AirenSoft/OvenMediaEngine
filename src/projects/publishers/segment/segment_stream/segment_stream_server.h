//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/publisher.h>
#include <config/config_manager.h>
#include <modules/http/http.h>
#include <monitoring/monitoring.h>

#include <memory>

#include "segment_stream_interceptor.h"
#include "segment_stream_observer.h"
#include "segment_stream_request_info.h"

class SegmentStreamServer
{
public:
	SegmentStreamServer();
	virtual ~SegmentStreamServer() = default;

	// thread_count: A thread count of SegmentWorkerManager
	// worker_count: A thread count of socket pool
	bool Start(
		const cfg::Server &server_config,
		const ov::SocketAddress *address,
		const ov::SocketAddress *tls_address,
		bool disable_http2_force,
		int thread_count,
		int worker_count);
	bool Stop();

	bool AppendCertificate(const std::shared_ptr<const info::Certificate> &certificate);
	bool RemoveCertificate(const std::shared_ptr<const info::Certificate> &certificate);

	bool AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

	bool Disconnect(const ov::String &app_name, const ov::String &stream_name);

	void SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list);

	virtual PublisherType GetPublisherType() const noexcept = 0;
	virtual const char *GetPublisherName() const noexcept = 0;

protected:
	virtual std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor();

	virtual bool PrepareInterceptors(
		const std::shared_ptr<http::svr::HttpServer> &http_server,
		const std::shared_ptr<http::svr::HttpsServer> &https_server,
		int thread_count, const SegmentProcessHandler &process_handler);

	bool ProcessRequest(const std::shared_ptr<http::svr::HttpExchange> &client);

	// Interfaces
	virtual bool ProcessStreamRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
															 const SegmentStreamRequestInfo &request_info,
															 const ov::String &file_ext) = 0;

	virtual bool ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
															   const SegmentStreamRequestInfo &request_info,
															   PlayListType play_list_type) = 0;

	virtual bool ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
															  const SegmentStreamRequestInfo &request_info,
															  SegmentType segment_type) = 0;

	std::shared_ptr<pub::Stream> GetStream(const std::shared_ptr<http::svr::HttpExchange> &client);
	std::shared_ptr<mon::StreamMetrics> GetStreamMetric(const std::shared_ptr<http::svr::HttpExchange> &client);

protected:
	std::shared_ptr<http::svr::HttpServer> _http_server;
	std::shared_ptr<http::svr::HttpsServer> _https_server;
	std::vector<std::shared_ptr<SegmentStreamObserver>> _observers;

	http::CorsManager _cors_manager;
};
