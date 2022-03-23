//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/client_socket.h>

#include "transactions/http_transaction.h"
#include "transactions/websocket_session.h"
#include "transactions/http2_stream.h"

//TODO(Getroot) : Move to Server.xml
#define HTTP_CONNECTION_TIMEOUT_MS		10 * 1000
#define WEBSOCKET_CONNECTION_TIMEOUT_MS	WEBSOCKET_PING_INTERVAL_MS * 3

namespace http
{
	namespace svr
	{
		class HttpServer;
		class HttpsServer;

		class HttpConnection : public ov::EnableSharedFromThis<HttpConnection>
		{
		public:
			HttpConnection(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &client_socket);
			
			void OnDataReceived(const std::shared_ptr<const ov::Data> &data);
			void Close(PhysicalPortDisconnectReason reason);

			bool OnRepeatTask();

			void SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data);
			void OnTlsAccepted();

			std::shared_ptr<ov::TlsServerData> GetTlsData() const;
			std::shared_ptr<ov::ClientSocket> GetSocket() const;
			ConnectionType GetConnectionType() const;
			std::shared_ptr<HttpTransaction> GetHttpTransaction() const;
			// Get Websocket Session
			std::shared_ptr<WebSocketSession> GetWebSocketSession() const;

			// Get ID
			uint32_t GetId() const;

			// Get interceptor, return cached interceptor
			std::shared_ptr<RequestInterceptor> GetInterceptor() const;
			// Find interceptor
			std::shared_ptr<RequestInterceptor> FindInterceptor(const std::shared_ptr<HttpTransaction> &transaction);

			// To string
			ov::String ToString() const;

		private:
			// For HTTP 1.0 and HTTP 1.1
			ssize_t OnHttp1RequestReceived(const std::shared_ptr<const ov::Data> &data);
			ssize_t OnHttp2RequestReceived(const std::shared_ptr<const ov::Data> &data);
			ssize_t OnWebSocketDataReceived(const std::shared_ptr<const ov::Data> &data);

			bool AddStream(const std::shared_ptr<HttpTransaction> &transaction);
			std::shared_ptr<HttpTransaction> FindHttpStream(uint32_t stream_id);
			bool DeleteHttpStream(uint32_t stream_id);

			bool UpgradeToWebSocket(const std::shared_ptr<HttpTransaction> &transaction);

			void CheckTimeout();
			
			// Default : HTTP 1.1
			ConnectionType _connection_type = ConnectionType::Http11;
			std::shared_ptr<HttpServer> _server = nullptr;
			std::shared_ptr<ov::ClientSocket> _client_socket = nullptr;
			std::shared_ptr<ov::TlsServerData> _tls_data;

			// For HTTP/1.1
			std::shared_ptr<HttpTransaction> _http_transaction = nullptr;

			// For HTTP/2.0
			// Stream Identifier, HttpTransaction
			std::map<uint32_t, std::shared_ptr<HttpStream>> _http_stream_map;

			// For Websocket
			std::shared_ptr<WebSocketSession> _websocket_session = nullptr;
			// Websocket Frame
			std::shared_ptr<ws::Frame> _websocket_frame = nullptr;

			std::shared_ptr<RequestInterceptor> _interceptor = nullptr;

			std::recursive_mutex _close_mutex;
		};
	} // namespace svr
} // namespace http