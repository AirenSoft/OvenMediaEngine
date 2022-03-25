//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http/server/http_server.h>

class TimeInterceptor : public http::svr::DefaultInterceptor
{
public:
	bool IsInterceptorForRequest(const std::shared_ptr<const http::svr::HttpExchange> &client) override;
};
