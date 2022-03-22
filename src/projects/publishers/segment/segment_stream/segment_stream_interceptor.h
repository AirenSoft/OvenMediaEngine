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

	ssize_t OnDataReceived(const std::shared_ptr<http::svr::HttpTransaction> &transaction, const std::shared_ptr<const ov::Data> &data) override;
	http::svr::InterceptorResult OnRequestCompleted(const std::shared_ptr<http::svr::HttpTransaction> &transaction) override;

	bool IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpTransaction> &client) override;

protected:
	SegmentWorkerManager _worker_manager;
};
