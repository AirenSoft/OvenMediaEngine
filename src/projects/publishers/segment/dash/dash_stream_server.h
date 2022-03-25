//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <publishers/segment/segment_stream/segment_stream_server.h>

#include "dash_interceptor.h"
#include "dash_packetizer.h"

class DashStreamServer : public SegmentStreamServer
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::Dash;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "DASH Publisher";
	}

protected:
	std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override;

	bool PrepareInterceptors(
		const std::shared_ptr<http::svr::HttpServer> &http_server,
		const std::shared_ptr<http::svr::HttpsServer> &https_server,
		int thread_count, const SegmentProcessHandler &process_handler) override;

	bool ProcessStreamRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
													 const SegmentStreamRequestInfo &request_info,
													 const ov::String &file_ext) override;

	bool ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
													   const SegmentStreamRequestInfo &request_info,
													   PlayListType play_list_type) override;

	bool ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const SegmentStreamRequestInfo &request_info,
													  SegmentType segment_type) override;
};
