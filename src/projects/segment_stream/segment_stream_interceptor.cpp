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

SegmentStreamInterceptor::SegmentStreamInterceptor(cfg::PublisherType publisher_type,
                                                    int thread_count,
                                                    const SegmentProcessHandler &process_handler)
{
    _publisher_type = publisher_type;
    _is_crossdomain_response = true;
    _worker_manager.Start(thread_count, process_handler);
}

SegmentStreamInterceptor::~SegmentStreamInterceptor()
{
    _worker_manager.Stop();
}

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request,
        const std::shared_ptr<const HttpResponse> &response)
{
	// logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 이상 체크
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

	// mpd/m4s
	if(_publisher_type == cfg::PublisherType::Dash)
    {
	    if( request->GetRequestTarget().IndexOf(".m4s") >= 0 ||
	        request->GetRequestTarget().IndexOf(".mpd") >= 0 ||
	        (_is_crossdomain_response && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
	        return true;
    }
	// m3u8/ts
	else if(_publisher_type == cfg::PublisherType::Hls)
    {
        if( request->GetRequestTarget().IndexOf(".m3u8") >= 0 ||
            request->GetRequestTarget().IndexOf(".ts") >= 0 ||
            (_is_crossdomain_response && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
            return true;
    }

	return false;
}

//====================================================================================================
// OnHttpData
// - (add thread) --> (SegmentStreamServer::ProcessRequest) function call
//====================================================================================================
bool SegmentStreamInterceptor::OnHttpData(const std::shared_ptr<HttpRequest> &request,
                                        const std::shared_ptr<HttpResponse> &response,
                                        const std::shared_ptr<const ov::Data> &data)
{
    const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);
    ssize_t current_length = (request_body != nullptr) ? request_body->GetLength() : 0L;
    ssize_t content_length = request->GetContentLength();

    std::shared_ptr<const ov::Data> process_data;
    if((content_length > 0) && ((current_length + static_cast<ssize_t>(data->GetLength())) > content_length))
    {
        if(content_length > current_length)
        {
            process_data = data->Subdata(0L, static_cast<size_t>(content_length - current_length));
        }
        else
        {
            return false;
        }
    }
    else
    {
        process_data = data;
    }

    if(process_data != nullptr)
    {
        request_body->Append(process_data.get());

        if(static_cast<ssize_t>(request_body->GetLength()) == content_length)
        {
            response->SetStatusCode(HttpStatusCode::OK);
            _worker_manager.AddWork(request, response);
        }

        return true;
    }

    return false;
}
