//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "config/items/items.h"
#include "http_server/http_server.h"
#include <list>
#include <thread>
#include "segment_worker_manager.h"

class SegmentStreamInterceptor : public HttpDefaultInterceptor
{
public:
    SegmentStreamInterceptor();
	~SegmentStreamInterceptor() ;

public:
    void Start(int thread_count, const SegmentProcessHandler &process_handler);

	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;

    void SetCrossdomainBlock() { _is_crossdomain_block = false; }

protected:

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;

protected :
    SegmentWorkerManager _worker_manager;
    bool _is_crossdomain_block;
};
