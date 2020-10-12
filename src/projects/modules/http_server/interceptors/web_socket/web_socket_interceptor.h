//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "web_socket_client.h"
#include "web_socket_frame.h"

#include <modules/http_server/http_server.h>
#include <shared_mutex>
class WebSocketInterceptor : public HttpRequestInterceptor
{
public:
	WebSocketInterceptor();
	~WebSocketInterceptor() override;

	// ws.on('connection');
	void SetConnectionHandler(WebSocketConnectionHandler handler);
	// ws.on('message');
	void SetMessageHandler(WebSocketMessageHandler handler);
	// ws.on('error');
	void SetErrorHandler(WebSocketErrorHandler handler);
	// ws.on('close');
	void SetCloseHandler(WebSocketCloseHandler handler);

protected:
	ov::DelayQueueAction DoPing(void *parameter);

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) override;

	// If these handler return false, the connection will be disconnected
	HttpInterceptorResult OnHttpPrepare(const std::shared_ptr<HttpClient> &client) override;
	HttpInterceptorResult OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpClient> &client, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpClient> &client) override;

	struct WebSocketInfo
	{
		WebSocketInfo(std::shared_ptr<WebSocketClient> response, std::shared_ptr<WebSocketFrame> frame)
			: response(response),
			  frame(frame)
		{
		}

		std::shared_ptr<WebSocketClient> response;
		std::shared_ptr<WebSocketFrame> frame;
	};

	std::shared_mutex _websocket_client_list_mutex;
	// key: request
	// value: websocket info
	std::map<const std::shared_ptr<HttpRequest>, std::shared_ptr<WebSocketInfo>> _websocket_client_list;

	WebSocketConnectionHandler _connection_handler;
	WebSocketMessageHandler _message_handler;
	WebSocketErrorHandler _error_handler;
	WebSocketCloseHandler _close_handler;

	ov::DelayQueue _ping_timer;
};
