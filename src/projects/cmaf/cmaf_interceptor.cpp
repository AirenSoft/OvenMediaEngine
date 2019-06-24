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

#define MPD_VIDEO_SUFFIX    "_video_cmaf.m4s"
#define MPD_AUDIO_SUFFIX    "_audio_cmaf.m4s"
#define PLAYLIST_FILE_NAME  "manifest_cmaf.mpd"
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
    if((request->GetRequestTarget().IndexOf(MPD_VIDEO_SUFFIX) >= 0) ||
        (request->GetRequestTarget().IndexOf(MPD_AUDIO_SUFFIX) >= 0) ||
        (request->GetRequestTarget().IndexOf(PLAYLIST_FILE_NAME) >= 0) ||
        (!_is_crossdomain_block && request->GetRequestTarget().IndexOf("crossdomain.xml") >= 0))
    {
        return true;
    }


	return false;
}