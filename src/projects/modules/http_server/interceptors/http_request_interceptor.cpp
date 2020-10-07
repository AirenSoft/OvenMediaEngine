//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_request_interceptor.h"

#include "../http_request.h"

const std::shared_ptr<ov::Data> &HttpRequestInterceptor::GetRequestBody(const std::shared_ptr<HttpRequest> &request)
{
	return request->GetRequestBodyInternal();
}
