//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http/server/http_server.h>

#include <shared_mutex>

#include "web_socket_client.h"
#include "web_socket_frame.h"

namespace http
{
	namespace svr
	{
		namespace ws
		{
			class Interceptor : public RequestInterceptor
			{
			public:
				Interceptor();
				~Interceptor() override;

				// ws.on('connection');
				void SetConnectionHandler(ConnectionHandler handler);
				// ws.on('message');
				void SetMessageHandler(MessageHandler handler);
				// ws.on('error');
				void SetErrorHandler(ErrorHandler handler);
				// ws.on('close');
				void SetCloseHandler(CloseHandler handler);

			protected:
				ov::DelayQueueAction DoPing(void *parameter);

				//--------------------------------------------------------------------
				// Implementation of HttpRequestInterceptorInterface
				//--------------------------------------------------------------------
				RequestConnectionType GetConnectionType() override
				{
					return RequestConnectionType::WebSocket;
				}
				bool IsInterceptorForRequest(const std::shared_ptr<const HttpConnection> &client) override;

				// If these handler return false, the connection will be disconnected
				InterceptorResult OnHttpPrepare(const std::shared_ptr<HttpConnection> &client) override;
				InterceptorResult OnHttpData(const std::shared_ptr<HttpConnection> &client, const std::shared_ptr<const ov::Data> &data) override;
				void OnHttpError(const std::shared_ptr<HttpConnection> &client, StatusCode status_code) override;
				void OnHttpClosed(const std::shared_ptr<HttpConnection> &client, PhysicalPortDisconnectReason reason) override;

				struct Info
				{
					Info(std::shared_ptr<Client> response, std::shared_ptr<Frame> frame)
						: response(response),
						  frame(frame)
					{
					}

					std::shared_ptr<Client> response;
					std::shared_ptr<Frame> frame;
				};

				std::shared_mutex _websocket_client_list_mutex;
				// key: request
				// value: websocket info
				std::map<const std::shared_ptr<HttpRequest>, std::shared_ptr<Info>> _websocket_client_list;

				ConnectionHandler _connection_handler;
				MessageHandler _message_handler;
				ErrorHandler _error_handler;
				CloseHandler _close_handler;

				ov::DelayQueue _ping_timer{"WSPing"};
			};
		}  // namespace ws
	}	   // namespace svr
}  // namespace http
