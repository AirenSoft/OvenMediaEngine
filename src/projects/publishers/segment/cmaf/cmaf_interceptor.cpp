//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_interceptor.h"
#include "../dash/dash_define.h"
#include "cmaf_private.h"

bool CmafInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client)
{
	if(SegmentStreamInterceptor::IsInterceptorForRequest(client) == false)
	{
		return false;
	}

	const auto request = client->GetRequest();

	// Temporary code to accept HTTP 1.0
	if ((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() < 1.0))
	{
		return false;
	}

	// Check file pattern
	// TODO(dimiden): Check this code later - IsInterceptorForRequest() should not filter method and filename. It is recommended to use Register()
	auto &request_target = request->GetRequestTarget();

	if (
		(request_target.IndexOf(CMAF_MPD_VIDEO_FULL_SUFFIX) >= 0) ||
		(request_target.IndexOf(CMAF_MPD_AUDIO_FULL_SUFFIX) >= 0) ||
		(request_target.IndexOf(CMAF_PLAYLIST_FULL_FILE_NAME) >= 0) ||
		((_is_crossdomain_block == false) && request_target.IndexOf(DASH_CORS_FILE_NAME) >= 0))
	{
		return true;
	}

	return false;
}