//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "time_interceptor.h"

#include "segment_stream_private.h"

bool TimeInterceptor::IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client)
{
	auto path = client->GetRequest()->GetRequestTarget();

	// TimeInterceptor only handles /time API

	auto index = path.IndexOf('?');

	if (index >= 0)
	{
		path = path.Substring(0, index);
	}

	return path == "/time";
}