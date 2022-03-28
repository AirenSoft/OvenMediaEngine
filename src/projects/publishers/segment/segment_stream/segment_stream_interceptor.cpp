//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream_interceptor.h"
#include "segment_stream_private.h"

SegmentStreamInterceptor::SegmentStreamInterceptor()
{
}

SegmentStreamInterceptor::~SegmentStreamInterceptor()
{
	_worker_manager.Stop();
}

bool SegmentStreamInterceptor::Start(int thread_count, const SegmentProcessHandler &process_handler)
{
	return _worker_manager.Start(thread_count, process_handler);
}

http::svr::InterceptorResult SegmentStreamInterceptor::OnRequestCompleted(const std::shared_ptr<http::svr::HttpExchange> &exchange)
{
	auto response = exchange->GetResponse();

	response->SetStatusCode(http::StatusCode::OK);
	_worker_manager.PushConnection(exchange);

	return http::svr::InterceptorResult::Moved;
}

bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client)
{
	auto request = client->GetRequest();
	
	if (request->GetConnectionType() == http::ConnectionType::Unknown || 
		request->GetConnectionType() == http::ConnectionType::WebSocket)
	{
		return false;
	}

	return true;
}