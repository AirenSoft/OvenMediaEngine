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

				class Info
				{
				public:
					Info(std::shared_ptr<Client> client, std::shared_ptr<Frame> frame)
						: _client(client),
						  _frame(frame)
					{
					}

					std::shared_ptr<Client> GetClient()
					{
						return _client;
					}

					std::shared_ptr<Frame> GetFrame()
					{
						if (_frame == nullptr)
						{
							_frame = std::make_shared<Frame>();
						}

						return _frame;
					}

				private:
					std::shared_ptr<Client> _client;
					// The frame that's currently being processed
					std::shared_ptr<Frame> _frame;
				};

				std::shared_mutex _websocket_client_list_mutex;
				// key: request
				// value: websocket info
				std::unordered_map<std::shared_ptr<HttpRequest>, std::shared_ptr<Info>> _websocket_client_list;

				ConnectionHandler _connection_handler;
				MessageHandler _message_handler;
				ErrorHandler _error_handler;
				CloseHandler _close_handler;

				std::shared_ptr<const ov::Data> _ping_data;
				ov::DelayQueue _ping_timer{"WSPing"};
			};
		}  // namespace ws
	}	   // namespace svr
}  // namespace http
