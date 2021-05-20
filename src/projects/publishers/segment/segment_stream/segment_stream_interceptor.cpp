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

//====================================================================================================
// OnHttpData
// - (add thread) --> (SegmentStreamServer::ProcessRequest) function call
// http 1.0  : request -> response
// http 1.1  : request -> response -> request -> response ...
//====================================================================================================
http::svr::InterceptorResult SegmentStreamInterceptor::OnHttpData(const std::shared_ptr<http::svr::HttpConnection> &client, const std::shared_ptr<const ov::Data> &data)
{
	auto request = client->GetRequest();
	auto response = client->GetResponse();

	if (request->GetContentLength() == 0)
	{
		response->SetStatusCode(http::StatusCode::OK);
		_worker_manager.AddWork(client, request->GetRequestTarget(), request->GetHeader("Origin"));
	}
	else
	{
		const std::shared_ptr<ov::Data> request_body = GetRequestBody(request);

		// data size over(error)
		if (request_body == nullptr ||
			request_body->GetLength() + data->GetLength() > request->GetContentLength())
		{
			return http::svr::InterceptorResult::Disconnect;
		}

		request_body->Append(data.get());

		// http data completed
		if (request_body->GetLength() == request->GetContentLength())
		{
			response->SetStatusCode(http::StatusCode::OK);
			_worker_manager.AddWork(client, request->GetRequestTarget(), request->GetHeader("Origin"));
		}
	}

	return http::svr::InterceptorResult::Keep;
}

bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpConnection> &client)
{
	auto request = client->GetRequest();
	
	if(request->GetConnectionType() != http::svr::RequestConnectionType::HTTP)
	{
		return false;
	}

	return true;
}