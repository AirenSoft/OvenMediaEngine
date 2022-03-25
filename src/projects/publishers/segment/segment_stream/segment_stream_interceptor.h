//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/items/items.h>
#include <modules/http/server/http_server.h>

#include <list>
#include <thread>

#include "segment_worker_manager.h"

class SegmentStreamInterceptor : public http::svr::DefaultInterceptor
{
public:
	SegmentStreamInterceptor();
	~SegmentStreamInterceptor() override;

	bool Start(int thread_count, const SegmentProcessHandler &process_handler);

	http::svr::InterceptorResult OnRequestCompleted(const std::shared_ptr<http::svr::HttpExchange> &exchange) override;
	bool IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client) override;

protected:
	SegmentWorkerManager _worker_manager;
};
