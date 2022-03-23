//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_transaction.h"
#include "../../protocols/websocket/web_socket_client.h"
#include "../../protocols/websocket/web_socket_frame.h"

#define WEBSOCKET_PING_INTERVAL_MS		20 * 1000

namespace http
{
	namespace svr
	{
		// For WebSocket
		class WebSocketSession : public HttpTransaction
		{
		public:
			friend class HttpConnection;

			// Only can be created by upgrade
			WebSocketSession(const std::shared_ptr<HttpTransaction> &transaction);
			virtual ~WebSocketSession() = default;

			// Go Upgrade
			bool Upgrade();

			// Ping
			bool Ping();

			// WebSocketSession must be called Release when disconnected
			void Release();

			// Get websocket client
			std::shared_ptr<ws::Client> GetClient() const;

		private:
			bool OnFrameReceived(const std::shared_ptr<const ws::Frame> &frame);
			std::shared_ptr<ws::Client> _websocket_client;
			std::shared_ptr<const ov::Data> _ping_data;
			ov::StopWatch	_ping_timer;
		};
	}  // namespace svr
}  // namespace http