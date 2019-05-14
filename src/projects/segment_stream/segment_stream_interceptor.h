//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once


#include "../config/items/items.h"
#include "../http_server/http_server.h"
#include <list>
#include <thread>
#include "segment_worker_manager.h"

class SegmentStreamInterceptor : public HttpDefaultInterceptor
{
public:
    SegmentStreamInterceptor(cfg::PublisherType publisher_type,
                            int thread_count,
                            const SegmentProcessHandler &process_handler);
	~SegmentStreamInterceptor() ;

public:

	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;
    void DisableCrossdomainResponse() { _is_crossdomain_response = false; }

protected:

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;

private :

    SegmentWorkerManager _worker_manager;
    cfg::PublisherType _publisher_type;
    bool _is_crossdomain_response;
};
