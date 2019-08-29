//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_interceptor.h"
#include "cmaf_private.h"
#include "../dash/dash_define.h"

CmafInterceptor::CmafInterceptor()
{

}

CmafInterceptor::~CmafInterceptor()
{

}

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool CmafInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request,
        const std::shared_ptr<const HttpResponse> &response)
{
	// Get Method 1.1 check
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

    // mpd/m4s
    if((request->GetRequestTarget().IndexOf(CMAF_MPD_VIDEO_FULL_SUFFIX) >= 0) ||
        (request->GetRequestTarget().IndexOf(CMAF_MPD_AUDIO_FULL_SUFFIX) >= 0) ||
        (request->GetRequestTarget().IndexOf(CMAF_PLAYLIST_FULL_FILE_NAME) >= 0) ||
        (!_is_crossdomain_block && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
    {
        return true;
    }


	return false;
}