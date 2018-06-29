//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "web_socket_request.h"
#include "web_socket_datastructure.h"
#include "../../http_private.h"

#include <unistd.h>
#include <algorithm>

WebSocketRequest::WebSocketRequest(const std::shared_ptr<HttpRequest> &request)
	: _request(request)
{
}

ov::String WebSocketRequest::ToString() const
{
	return ov::String::FormatString("<WebSocketRequest: %p>", this);
}
