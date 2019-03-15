//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "web_socket_frame.h"
#include "web_socket_client.h"

#include <http_server/http_server.h>

class WebSocketInterceptor : public HttpRequestInterceptor
{
public:
	WebSocketInterceptor()
		: _connection_handler(nullptr),
		  _message_handler(nullptr),
		  _error_handler(nullptr),
		  _close_handler(nullptr)
	{
	}

	~WebSocketInterceptor();

	// ws.on('connection');
	void SetConnectionHandler(WebSocketConnectionHandler handler);
	// ws.on('message');
	void SetMessageHandler(WebSocketMessageHandler handler);
	// ws.on('error');
	void SetErrorHandler(WebSocketErrorHandler handler);
	// ws.on('close');
	void SetCloseHandler(WebSocketCloseHandler handler);

protected:
	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;

	// If these handler return false, the connection will be disconnected
	bool OnHttpPrepare(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;
	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;

	struct WebSocketInfo
	{
		std::shared_ptr<WebSocketClient> response;
		std::shared_ptr<WebSocketFrame> frame;
	};

	// key: request
	// value: websocket info
	std::map<const std::shared_ptr<HttpRequest>, WebSocketInfo> _websocket_client_list;

	WebSocketConnectionHandler _connection_handler;
	WebSocketMessageHandler _message_handler;
	WebSocketErrorHandler _error_handler;
	WebSocketCloseHandler _close_handler;
};
