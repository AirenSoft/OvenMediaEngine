//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./web_socket_datastructure.h"

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>
#include <http_server/http_server.h>

class WebSocketRequest
{
public:
	explicit WebSocketRequest(const std::shared_ptr<HttpRequest> &request);

	// TODO: 나중에 정리하기
	const std::shared_ptr<HttpRequest> &GetRequest()
	{
		return _request;
	}

	ov::String ToString() const;

protected:
	std::shared_ptr<HttpRequest> _request;
};
