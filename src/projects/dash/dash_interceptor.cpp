//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_interceptor.h"
#include "dash_private.h"

DashInterceptor::DashInterceptor()
{

}

DashInterceptor::~DashInterceptor()
{

}

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool DashInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request,
        const std::shared_ptr<const HttpResponse> &response)
{
	// logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 이상 체크
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

    // mpd/m4s
    if((request->GetRequestTarget().IndexOf(".m4s") >= 0) ||
       (request->GetRequestTarget().IndexOf(".mpd") >= 0) ||
       (!_is_crossdomain_block && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
    {
        return true;
    }


	return false;
}