//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "websocket_session.h"
#include "http_connection.h"

#define OV_LOG_TAG		"WebSocket"

namespace http
{
	namespace svr
	{
			// For Upgrade
			WebSocketSession::WebSocketSession(const std::shared_ptr<HttpTransaction> &transaction)
				: HttpTransaction(transaction)
			{
				ov::String str("OvenMediaEngine");
				_ping_data = str.ToData(false);

				_request->SetConnectionType(ConnectionType::WebSocket);
			}

			void WebSocketSession::Release()
			{
				// To solve cyclic reference
				_websocket_client.reset();
			}

			std::shared_ptr<ws::Client> WebSocketSession::GetClient() const
			{
				return _websocket_client;
			}

			// Go Upgrade
			bool WebSocketSession::Upgrade()
			{
				_websocket_client = std::make_shared<ws::Client>(GetSharedPtr());

				// Find Interceptor
				auto interceptor = _connection->FindInterceptor(GetSharedPtr());
				if (interceptor == nullptr)
				{
					SetStatus(Status::Error);
					_response->SetStatusCode(StatusCode::NotFound);
					_response->Response();
					return false;
				}

				interceptor->OnRequestPrepared(GetSharedPtr());

				SetStatus(HttpTransaction::Status::Exchanging);

				return true;
			}

			bool WebSocketSession::Ping()
			{
				if (_websocket_client == nullptr)
				{
					return false;
				}

				return _websocket_client->Send(_ping_data, ws::FrameOpcode::Ping) > 0;
			}

			bool WebSocketSession::OnFrameReceived(const std::shared_ptr<const ws::Frame> &frame)
			{
				auto interceptor = _connection->FindInterceptor(GetSharedPtr());
				if (interceptor == nullptr)
				{
					SetStatus(Status::Error);
					return false;
				}
				const std::shared_ptr<const ov::Data> payload = frame->GetPayload();

				switch (static_cast<ws::FrameOpcode>(frame->GetHeader().opcode))
				{
					case ws::FrameOpcode::ConnectionClose:
						// The client requested close the connection
						logtd("Client requested close connection: reason:\n%s", payload->Dump("Reason").CStr());
						interceptor->OnRequestCompleted(GetSharedPtr());
						SetStatus(HttpTransaction::Status::Completed);
						return true;

					case ws::FrameOpcode::Ping:
						logtd("A ping frame is received:\n%s", payload->Dump().CStr());
						// Send a pong frame to the client
						_websocket_client->Send(payload, ws::FrameOpcode::Pong);
						break;

					case ws::FrameOpcode::Pong:
						// Ignore pong frame
						logtd("A pong frame is received:\n%s", payload->Dump().CStr());
						break;

					default:
						logtd("%s:\n%s", frame->ToString().CStr(), payload->Dump("Frame", 0L, 1024L, nullptr).CStr());
						auto consumed_bytes = interceptor->OnDataReceived(GetSharedPtr(), payload);
						if (consumed_bytes < 0 || static_cast<size_t>(consumed_bytes) != payload->GetLength())
						{
							SetStatus(Status::Error);
							return false;
						}
						break;
				}

				return true;
			}
	}  // namespace svr
}  // namespace http