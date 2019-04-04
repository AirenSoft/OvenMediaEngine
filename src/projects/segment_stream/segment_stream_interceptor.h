//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <http_server/http_server.h>

class SegmentStreamInterceptor : public HttpDefaultInterceptor
{
public:
	SegmentStreamInterceptor()
	{
	}

	~SegmentStreamInterceptor() = default;

	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;

protected:

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;
};
