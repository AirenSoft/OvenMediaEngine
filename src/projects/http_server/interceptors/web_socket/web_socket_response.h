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

class WebSocketResponse
{
public:
	WebSocketResponse(ov::ClientSocket *remote, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);
	virtual ~WebSocketResponse();

	ssize_t Send(const std::shared_ptr<const ov::Data> &data, WebSocketFrameOpcode opcode);
	ssize_t Send(const std::shared_ptr<const ov::Data> &data);
	ssize_t Send(const ov::String &string);

	// TODO: 나중에 숨기기
	const std::shared_ptr<HttpRequest> &GetRequest()
	{
		return _request;
	}

	// TODO: 나중에 숨기기
	const std::shared_ptr<HttpResponse> &GetResponse()
	{
		return _response;
	}

	ov::String ToString() const;

	void Close();

protected:
	ov::ClientSocket *_remote;

	std::shared_ptr<HttpRequest> _request;
	std::shared_ptr<HttpResponse> _response;
};
