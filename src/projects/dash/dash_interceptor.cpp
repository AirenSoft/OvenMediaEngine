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
	// Get Method 1.1 check
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

    // mpd/m4s
    if((request->GetRequestTarget().IndexOf(DASH_MPD_VIDEO_SUFFIX) >= 0) ||
       (request->GetRequestTarget().IndexOf(DASH_MPD_AUDIO_SUFFIX) >= 0) ||
       (request->GetRequestTarget().IndexOf(DASH_PLAYLIST_FILE_NAME) >= 0) ||
       (!_is_crossdomain_block && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
    {
        return true;
    }


	return false;
}