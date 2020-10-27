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
	_is_crossdomain_block = false;
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
HttpInterceptorResult SegmentStreamInterceptor::OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data)
{
	auto request = client->GetRequest();
	auto response = client->GetResponse();

	OV_ASSERT2(request->GetContentLength() >= 0);

	if (request->GetContentLength() == 0)
	{
		response->SetStatusCode(HttpStatusCode::OK);
		_worker_manager.AddWork(client, request->GetRequestTarget(), request->GetHeader("Origin"));
	}
	else
	{
		const std::shared_ptr<ov::Data> request_body = GetRequestBody(request);

		// data size over(error)
		if (request_body == nullptr ||
			request_body->GetLength() + data->GetLength() > static_cast<size_t>(request->GetContentLength()))
		{
			return HttpInterceptorResult::Disconnect;
		}

		request_body->Append(data.get());

		// http data completed
		if (request_body->GetLength() == static_cast<size_t>(request->GetContentLength()))
		{
			response->SetStatusCode(HttpStatusCode::OK);
			_worker_manager.AddWork(client, request->GetRequestTarget(), request->GetHeader("Origin"));
		}
	}

	return HttpInterceptorResult::Keep;
}
