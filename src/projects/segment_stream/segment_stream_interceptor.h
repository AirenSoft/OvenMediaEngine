//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../http_server/http_server.h"
#include <list>
#include <thread>
#include "segment_worker_manager.h"
struct ThreadChecker
{
    std::shared_ptr<std::thread> thread = nullptr;
    bool is_closed = false;
};

class SegmentStreamInterceptor : public HttpDefaultInterceptor
{
public:
    SegmentStreamInterceptor(int thread_count, const SegmentProcessHandler &process_handler);
	~SegmentStreamInterceptor() ;

	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;

protected:

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;

private :

   SegmentWorkerManager _worker_manager;
};
