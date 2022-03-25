//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "monitoring_interceptor.h"

#define OV_LOG_TAG "Monitoring"

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool MonitoringInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client)
{
	const auto request = client->GetRequest();
	
	// logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 check
	if(request->GetMethod() != http::Method::Get || request->GetHttpVersionAsNumber() <= 1.0)
	{
		return false;
	}

	return true;
}
