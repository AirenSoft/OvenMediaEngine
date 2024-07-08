//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "thumbnail_interceptor.h"
#include "thumbnail_private.h"

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool ThumbnailInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client)
{
	const auto request = client->GetRequest();
	
	// logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 check
	if(request->GetMethod() != http::Method::Get)
	{
		return false;
	}

	// ts/m3u8
	if ((request->GetRequestTarget().LowerCaseString().IndexOf(".jpg") >= 0) ||
		(request->GetRequestTarget().LowerCaseString().IndexOf(".png") >= 0))
	{
		return true;
	}

	return false;
}
