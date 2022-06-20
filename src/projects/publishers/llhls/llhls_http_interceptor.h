//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <modules/http/server/http_default_interceptor.h>

class LLHlsHttpInterceptor : public http::svr::DefaultInterceptor
{
protected:
	bool IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &exchange) override
	{
		auto request = exchange->GetRequest();

		if (request->GetMethod() == http::Method::Get || request->GetMethod() == http::Method::Options)
		{
			// playlist.m3u8 is for legacy HLS
			if ((request->GetRequestTarget() != "playlist.m3u8"	&& request->GetRequestTarget().IndexOf(".m3u8") >= 0) || 
				request->GetRequestTarget().IndexOf("llhls.m4s") >= 0)
			{
				return true;
			}
		}

		return false;
	}
};
