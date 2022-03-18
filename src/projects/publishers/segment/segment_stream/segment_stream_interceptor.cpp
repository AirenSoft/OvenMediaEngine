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

ssize_t SegmentStreamInterceptor::OnDataReceived(const std::shared_ptr<http::svr::HttpTransaction> &transaction, const std::shared_ptr<const ov::Data> &data)
{
	auto request = transaction->GetRequest();
	auto response = transaction->GetResponse();
	ssize_t consumed_bytes = 0;
	
	const std::shared_ptr<ov::Data> request_body = GetRequestBody(request);

	//TODO(h2) : Data size can be over, 아래 코드가 정상 동작하는지 체크해야 한다. 
	if (request_body == nullptr)
	{
		return -1;
	}

	if (request_body->GetLength() + data->GetLength() > request->GetContentLength())
	{
		consumed_bytes = request->GetContentLength() - request_body->GetLength();
	}
	else
	{
		consumed_bytes = data->GetLength();
	}

	auto process_data = data->Subdata(0L, consumed_bytes);

	request_body->Append(process_data);

	return consumed_bytes;
}

http::svr::InterceptorResult SegmentStreamInterceptor::OnRequestCompleted(const std::shared_ptr<http::svr::HttpTransaction> &transaction)
{
	auto response = transaction->GetResponse();

	response->SetStatusCode(http::StatusCode::OK);
	_worker_manager.PushConnection(transaction);

	return http::svr::InterceptorResult::Moved;
}

bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpTransaction> &client)
{
	auto request = client->GetRequest();
	
	if (request->GetConnectionType() == http::ConnectionType::Unknown || 
		request->GetConnectionType() == http::ConnectionType::WebSocket)
	{
		return false;
	}

	return true;
}