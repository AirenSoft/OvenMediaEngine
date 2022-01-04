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
#include <modules/http/server/http_server.h>
#include <modules/http/server/https_server.h>
#include <modules/http/server/interceptors/http_request_interceptors.h>
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
		int thread_count,
		int worker_count);
	bool Stop();

	bool AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

	bool Disconnect(const ov::String &app_name, const ov::String &stream_name);

	// url_list can contains such values:
	//   - *: Adds "Access-Control-Allow-Origin: *" for the request
	//        If "*" and <domain> are passed at the same time, the <domain> value is ignored.
	//   - null: Adds "Access-Control-Allow-Origin: null" for the request
	//        If "null" and <domain> are passed at the same time, the "null" value is ignored.
	//   - <domain>: Adds "Access-Control-Allow-Origin: <domain>" for the request
	//
	// Empty url_list means 'Do not set any CORS header'
	//
	// NOTE - SetCrossDomains() isn't thread-safe.
	void SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list);

	virtual PublisherType GetPublisherType() const noexcept = 0;
	virtual const char *GetPublisherName() const noexcept = 0;

protected:
	virtual std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor();

	virtual bool PrepareInterceptors(
		const std::shared_ptr<http::svr::HttpServer> &http_server,
		const std::shared_ptr<http::svr::HttpsServer> &https_server,
		int thread_count, const SegmentProcessHandler &process_handler);

	bool ProcessRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
						const ov::String &request_target,
						const ov::String &origin_url);

	bool SetRtmpCorsHeaders(const std::shared_ptr<http::svr::HttpResponse> &response);
	bool SetHttpCorsHeaders(const info::VHostAppName &vhost_app_name, const ov::String &origin_url, const std::shared_ptr<const ov::Url> &url, const std::shared_ptr<http::svr::HttpResponse> &response);

	// Interfaces
	virtual http::svr::ConnectionPolicy ProcessStreamRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
															 const SegmentStreamRequestInfo &request_info,
															 const ov::String &file_ext) = 0;

	virtual http::svr::ConnectionPolicy ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
															   const SegmentStreamRequestInfo &request_info,
															   PlayListType play_list_type) = 0;

	virtual http::svr::ConnectionPolicy ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
															  const SegmentStreamRequestInfo &request_info,
															  SegmentType segment_type) = 0;

	bool UrlExistCheck(const std::vector<ov::String> &url_list, const ov::String &check_url);

	std::shared_ptr<pub::Stream> GetStream(const std::shared_ptr<http::svr::HttpConnection> &client);
	std::shared_ptr<mon::StreamMetrics> GetStreamMetric(const std::shared_ptr<http::svr::HttpConnection> &client);

protected:
	// https://fetch.spec.whatwg.org/#http-access-control-allow-origin
	// `Access-Control-Allow-Origin`
	// Indicates whether the response can be shared, via returning the literal value of the `Origin` request header
	// (which can be `null`) or `*` in a response.
	//
	// For example:
	// Access-Control-Allow-Origin: *
	// Access-Control-Allow-Origin: null
	// Access-Control-Allow-Origin: <origin>
	enum class CorsPolicy
	{
		// Do not add any CORS header
		Empty,
		// *
		All,
		// null
		Null,
		// Specific domain in _cors_http_map
		Origin
	};

	std::shared_ptr<http::svr::HttpServer> _http_server;
	std::shared_ptr<http::svr::HttpServer> _https_server;
	std::vector<std::shared_ptr<SegmentStreamObserver>> _observers;

	std::mutex _cors_mutex;
	
	std::unordered_map<info::VHostAppName, CorsPolicy> _cors_policy_map;
	// CORS for HTTP
	// key: VHostAppName, value: regex
	std::unordered_map<info::VHostAppName, std::vector<ov::Regex>> _cors_regex_list_map;
	// CORS for RTMP
	//
	// NOTE - The RTMP CORS setting follows the first declared <CrossDomains> setting,
	//        because crossdomain.xml must be located / and cannot be declared per app.
	ov::String _cors_rtmp;
};
