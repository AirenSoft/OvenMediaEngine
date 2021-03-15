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
#include <modules/http_server/http_server.h>
#include <modules/http_server/https_server.h>
#include <modules/http_server/interceptors/http_request_interceptors.h>

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
		const ov::SocketAddress *address,
		const ov::SocketAddress *tls_address,
		int thread_count,
		int worker_count);
	bool Stop();

	bool AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

	bool Disconnect(const ov::String &app_name, const ov::String &stream_name);

	void SetCrossDomain(const std::vector<ov::String> &url_list);

	virtual PublisherType GetPublisherType() const noexcept = 0;
	virtual const char *GetPublisherName() const noexcept = 0;
	virtual std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor()
	{
		return std::make_shared<SegmentStreamInterceptor>();
	}

protected:
	bool ParseRequestUrl(const ov::String &request_url,
						 ov::String &app_name, ov::String &stream_name,
						 ov::String &file_name, ov::String &file_ext);

	bool ProcessRequest(const std::shared_ptr<HttpClient> &client,
						const ov::String &request_target,
						const ov::String &origin_url);

	bool SetAllowOrigin(const ov::String &origin_url, const std::shared_ptr<HttpResponse> &response);

	// Interfaces
	virtual HttpConnection ProcessStreamRequest(const std::shared_ptr<HttpClient> &client,
												const SegmentStreamRequestInfo &request_info,
												const ov::String &file_ext) = 0;

	virtual HttpConnection ProcessPlayListRequest(const std::shared_ptr<HttpClient> &client,
												  const SegmentStreamRequestInfo &request_info,
												  PlayListType play_list_type) = 0;

	virtual HttpConnection ProcessSegmentRequest(const std::shared_ptr<HttpClient> &client,
												 const SegmentStreamRequestInfo &request_info,
												 SegmentType segment_type) = 0;

	bool UrlExistCheck(const std::vector<ov::String> &url_list, const ov::String &check_url);

	bool IncreaseBytesOut(const std::shared_ptr<HttpClient> &client, size_t sent_bytes);

protected:
	std::shared_ptr<HttpServer> _http_server;
	std::shared_ptr<HttpServer> _https_server;
	std::vector<std::shared_ptr<SegmentStreamObserver>> _observers;
	std::vector<ov::String> _cors_urls;
	ov::String _cross_domain_xml;
};
