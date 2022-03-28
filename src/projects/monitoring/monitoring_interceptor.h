//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <modules/http/server/http_server.h>

class MonitoringInterceptor : public http::svr::DefaultInterceptor
{
public:
    MonitoringInterceptor()
	{
	}

	~MonitoringInterceptor() = default;

protected:

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client) override;
};
