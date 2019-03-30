#include <utility>

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

class WebSocketClient
{
public:
	WebSocketClient(std::shared_ptr<ov::ClientSocket> remote, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);
	virtual ~WebSocketClient();

	ssize_t Send(const std::shared_ptr<const ov::Data> &data, WebSocketFrameOpcode opcode);
	ssize_t Send(const std::shared_ptr<const ov::Data> &data);
	ssize_t Send(const ov::String &string);
	ssize_t Send(const Json::Value &value);

	const std::shared_ptr<HttpRequest> &GetRequest()
	{
		return _request;
	}

	const std::shared_ptr<HttpResponse> &GetResponse()
	{
		return _response;
	}

	ov::String ToString() const;

	void Close();

protected:
	std::shared_ptr<ov::ClientSocket> _remote;

	std::shared_ptr<HttpRequest> _request;
	std::shared_ptr<HttpResponse> _response;
};
