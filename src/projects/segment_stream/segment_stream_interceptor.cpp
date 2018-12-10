//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream_interceptor.h"

#define OV_LOG_TAG "SegmentStream"

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response)
{
	logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 이상 체크
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

	return true;
}
