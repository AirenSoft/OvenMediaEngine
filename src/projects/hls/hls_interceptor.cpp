//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_interceptor.h"
#include "hls_private.h"

HlsInterceptor::HlsInterceptor()
{

}

HlsInterceptor::~HlsInterceptor()
{

}

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool HlsInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request,
        const std::shared_ptr<const HttpResponse> &response)
{
	// logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 이상 체크
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

    // ts/m3u8
    if((request->GetRequestTarget().IndexOf(".ts") >= 0) ||
       (request->GetRequestTarget().IndexOf(".m3u8") >= 0) ||
       (!_is_crossdomain_block && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
    {
        return true;
    }


	return false;
}