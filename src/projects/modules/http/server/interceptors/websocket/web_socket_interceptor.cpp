//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./web_socket_interceptor.h"

#include <base/ovcrypto/ovcrypto.h>
#include <modules/http/server/http_server.h>

#include <utility>

#include "../../../protocols/websocket/web_socket_datastructure.h"
#include "../../../protocols/websocket/web_socket_frame.h"
#include "../../../protocols/websocket/web_socket_private.h"

#define WEBSOCKET_ERROR_DOMAIN "WebSocket"

namespace http
{
	namespace svr
	{
		namespace ws
		{
			Interceptor::Interceptor()
			{
			}

			Interceptor::~Interceptor()
			{
			}

			bool Interceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpTransaction> &client)
			{
				const auto request = client->GetRequest();

				if (request->GetConnectionType() != ConnectionType::WebSocket)
				{
					logtd("%s is not websocket request", request->ToString().CStr());
					return false;
				}

				return true;
			}

			bool Interceptor::OnRequestPrepared(const std::shared_ptr<HttpTransaction> &transaction)
			{
				auto websocket_session = std::dynamic_pointer_cast<WebSocketSession>(transaction);
				if (websocket_session == nullptr)
				{
					logtw("%s is not websocket session", transaction->GetConnection()->ToString().CStr());
					return false;
				}

				auto client = websocket_session->GetClient();
				if (client == nullptr)
				{
					logtw("%s has not websocket client", transaction->GetConnection()->ToString().CStr());
					return false;
				}

				if (_connection_handler != nullptr)
				{
					return _connection_handler(client);
				}
				else
				{
					logte("WebSocket interceptor has not connection_handler");
					return false;
				}

				return true;
			}

			ssize_t Interceptor::OnDataReceived(const std::shared_ptr<HttpTransaction> &transaction, const std::shared_ptr<const ov::Data> &data)
			{
				auto websocket_session = std::dynamic_pointer_cast<WebSocketSession>(transaction);
				if (websocket_session == nullptr)
				{
					logtw("%s is not websocket session", transaction->GetConnection()->ToString().CStr());
					return false;
				}

				auto client = websocket_session->GetClient();
				
				// Callback the payload
				if (_message_handler != nullptr)
				{
					if (_message_handler(client, data) == false)
					{
						return -1;
					}
				}

				return data->GetLength();
			}

			InterceptorResult Interceptor::OnRequestCompleted(const std::shared_ptr<HttpTransaction> &transaction)
			{
				return InterceptorResult::Completed;
			}

			void Interceptor::OnError(const std::shared_ptr<HttpTransaction> &transaction, StatusCode status_code)
			{
				auto websocket_session = std::dynamic_pointer_cast<WebSocketSession>(transaction);
				auto client = websocket_session->GetClient();

				if ((_error_handler != nullptr))
				{
					_error_handler(client, ov::Error::CreateError(WEBSOCKET_ERROR_DOMAIN, static_cast<int>(status_code), "%s", StringFromStatusCode(status_code)));
				}
			}

			void Interceptor::OnClosed(const std::shared_ptr<HttpConnection> &connection, PhysicalPortDisconnectReason reason)
			{
				auto websocket_session = connection->GetWebSocketSession();
				if (websocket_session == nullptr)
				{
					logtw("%s is not websocket connection", connection->ToString().CStr());
					return;
				}

				auto client = websocket_session->GetClient();

				if (_close_handler != nullptr)
				{
					_close_handler(client, reason);
				}
			}

			void Interceptor::SetConnectionHandler(ConnectionHandler handler)
			{
				_connection_handler = std::move(handler);
			}

			void Interceptor::SetMessageHandler(MessageHandler handler)
			{
				_message_handler = std::move(handler);
			}

			void Interceptor::SetErrorHandler(ErrorHandler handler)
			{
				_error_handler = std::move(handler);
			}

			void Interceptor::SetCloseHandler(CloseHandler handler)
			{
				_close_handler = std::move(handler);
			}
		}  // namespace ws
	}	   // namespace svr
}  // namespace http
