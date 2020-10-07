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
#include <modules/http_server/http_server.h>

class WebSocketClient
{
public:
	WebSocketClient(const std::shared_ptr<HttpClient> &client);
	virtual ~WebSocketClient();

	ssize_t Send(const std::shared_ptr<const ov::Data> &data, WebSocketFrameOpcode opcode);
	ssize_t Send(const std::shared_ptr<const ov::Data> &data);
	ssize_t Send(const ov::String &string);
	ssize_t Send(const Json::Value &value);

	const std::shared_ptr<HttpClient> &GetClient()
	{
		return _client;
	}

	ov::String ToString() const;

	void Close();

protected:
	std::shared_ptr<HttpClient> _client;
};
