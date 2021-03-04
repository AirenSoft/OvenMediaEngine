//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "modules/http_server/interceptors/web_socket/web_socket_interceptor.h"

class WebRtcPublisherSignallingInterceptor : public WebSocketInterceptor
{
public:
    //--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) override
	{
		if(WebSocketInterceptor::IsInterceptorForRequest(client) == false)
		{
			return false;
		}

		auto request = client->GetRequest();
		auto uri = request->GetUri();
		auto parsed_url = ov::Url::Parse(uri);
		if(parsed_url == nullptr)
		{
			return false;
		}

		// Accept if URL has not "direction" query string or has not "direction=send"
		// ws://host:port/app/stream
		// ws://host:port/app/stream?direction=recv
		// ws://host:port/app/stream?direction=1je921
		auto direction = parsed_url->GetQueryValue("direction");
		if(direction.IsEmpty() || direction != "send")
		{
			return true;
		}

		return false;
	}
};
