//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <modules/http_server/http_server.h>

class MonitoringInterceptor : public HttpDefaultInterceptor
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
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) override;
};
