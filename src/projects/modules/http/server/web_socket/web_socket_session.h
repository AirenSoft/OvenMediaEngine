//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../protocol/web_socket/web_socket_frame.h"
#include "../http_exchange.h"
#include "web_socket_client.h"

#define WEBSOCKET_PING_INTERVAL_MS 20 * 1000

namespace http
{
	namespace svr
	{
		namespace ws
		{
			// For WebSocket
			class WebSocketSession : public HttpExchange
			{
			public:
				friend class HttpConnection;

				// Only can be created by upgrade
				WebSocketSession(const std::shared_ptr<HttpExchange> &exchange);
				virtual ~WebSocketSession() = default;

				// Go Upgrade
				bool Upgrade();

				// Ping
				bool Ping();

				// WebSocketSession must be called Release when disconnected
				void Release();

				// Get websocket client
				std::shared_ptr<ws::Client> GetClient() const;

				bool OnFrameReceived(const std::shared_ptr<const prot::ws::Frame> &frame);

			private:
				std::shared_ptr<ws::Client> _websocket_client;
				std::shared_ptr<const ov::Data> _ping_data;
				ov::StopWatch _ping_timer;
			};
		}  // namespace ws
	} // namespace svr
}  // namespace http