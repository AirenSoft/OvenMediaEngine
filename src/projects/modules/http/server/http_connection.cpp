#include <base/ovcrypto/openssl/tls_server_data.h>

#include "http_server.h"
#include "http_connection.h"

#include "../http_private.h"

namespace http
{
	namespace svr
	{
		HttpConnection::HttpConnection(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &client_socket)
		{
			_server = server;
			_client_socket = client_socket;
			_connection_type = ConnectionType::Http11; // Default to HTTP/1.1
		}

		// Get Id from socket
		uint32_t HttpConnection::GetId() const
		{
			return _client_socket->GetNativeHandle();
		}

		// To string
		ov::String HttpConnection::ToString() const
		{
			// Print connection type, client address, client port, use of tls
			return ov::String::FormatString("HttpConnection(%p) : %s %s TLS(%s)",
				this,
				StringFromConnectionType(_connection_type).CStr(),
				_client_socket->ToString().CStr(),
				_tls_data ? "Enabled" : "Disabled");
		}
		
		// Called every 5 seconds
		bool HttpConnection::OnRepeatTask()
		{
			if (_connection_type == ConnectionType::WebSocket)
			{
				if (_websocket_session != nullptr)
				{
					_websocket_session->Ping();
				}
			}

			CheckTimeout();

			return true;
		}

		void HttpConnection::CheckTimeout()
		{
			// Check timeout 
			auto current = std::chrono::high_resolution_clock::now();
			auto elapsed_time_from_last_sent = std::chrono::duration_cast<std::chrono::milliseconds>(current - _client_socket->GetLastSentTime()).count();
			auto elapsed_time_from_last_recv = std::chrono::duration_cast<std::chrono::milliseconds>(current - _client_socket->GetLastRecvTime()).count();

			switch (_connection_type)
			{
				case ConnectionType::Http10:
				case ConnectionType::Http11:
				case ConnectionType::Http20:
					if (std::min(elapsed_time_from_last_recv, elapsed_time_from_last_sent) > HTTP_CONNECTION_TIMEOUT_MS)
					{
						// Close connection
						logti("Client(%s - %s) has timed out", StringFromConnectionType(_connection_type).CStr(), _client_socket->ToString().CStr());
						Close(PhysicalPortDisconnectReason::Disconnect);
					}
					break;
				case ConnectionType::WebSocket:
					// In websocket, if Pong does not arrive for a certain period of time, timeout should be processed. (It takes a very long time to check disconnected by ping transmission because it can be mistaken for sending a ping to a dead client.)
					if (elapsed_time_from_last_recv > WEBSOCKET_CONNECTION_TIMEOUT_MS)
					{
						// Close connection
						logti("Client(%s - %s) has timed out", StringFromConnectionType(_connection_type).CStr(),_client_socket->ToString().CStr());
						Close(PhysicalPortDisconnectReason::Disconnect);
					}
				default:
					return;
			}
		}

		// Called from HttpsServer
		void HttpConnection::SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data)
		{
			_tls_data = tls_data;
			if (_tls_data->GetSelectedAlpnProtocol() == ov::TlsServerData::AlpnProtocol::Http20)
			{
				_connection_type = ConnectionType::Http20;
			}
			else
			{
				_connection_type = ConnectionType::Http11;
			}
		}

		// Get Last Transaction
		std::shared_ptr<HttpTransaction> HttpConnection::GetHttpTransaction() const
		{
			return _http_transaction;
		}

		std::shared_ptr<WebSocketSession> HttpConnection::GetWebSocketSession() const
		{
			return _websocket_session;
		}
		
		// Get Interceptor
		std::shared_ptr<RequestInterceptor> HttpConnection::GetInterceptor() const
		{
			return _interceptor;
		}

		std::shared_ptr<ov::TlsServerData> HttpConnection::GetTlsData() const
		{
			return _tls_data;
		}

		std::shared_ptr<ov::ClientSocket> HttpConnection::GetSocket() const 
		{
			return _client_socket;
		}

		ConnectionType HttpConnection::GetConnectionType() const
		{
			return _connection_type;
		}

		// Find Interceptor
		std::shared_ptr<RequestInterceptor> HttpConnection::FindInterceptor(const std::shared_ptr<HttpTransaction> &transaction)
		{
			if (_interceptor != nullptr)
			{
				return _interceptor;
			}
			else 
			{
				// Cache interceptor
				_interceptor = _server->FindInterceptor(transaction);
			}

			// Find interceptor from server
			return _interceptor;
		}

		bool HttpConnection::UpgradeToWebSocket(const std::shared_ptr<HttpTransaction> &transaction)
		{
			_connection_type = ConnectionType::WebSocket;
					
			_websocket_session = std::make_shared<WebSocketSession>(_http_transaction);
			_websocket_session->Upgrade();

			return true;
		}

		void HttpConnection::Close(PhysicalPortDisconnectReason reason)
		{
			// mutex
			std::lock_guard<std::recursive_mutex> lock(_close_mutex);

			if (_interceptor != nullptr)
			{
				_interceptor->OnClosed(GetSharedPtr(), reason);
			}

			if (_http_transaction != nullptr)
			{
				_http_transaction.reset();
			}

			if (_websocket_frame != nullptr)
			{
				_websocket_frame.reset();
			}

			if (_websocket_session != nullptr)
			{
				_websocket_session->Release();
				_websocket_session.reset();
			}
			
			_http_stream_map.clear();

			if (reason != PhysicalPortDisconnectReason::Disconnected)
			{
				_client_socket->Close();
			}
		}

		void HttpConnection::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
		{
			std::lock_guard<std::recursive_mutex> lock(_close_mutex);

			auto process_data = data->Clone();
			while (process_data->GetLength() > 0)
			{
				ssize_t processed_length = 0;

				switch (_connection_type)
				{
					case ConnectionType::Http10:
					case ConnectionType::Http11:
						processed_length = OnHttp1RequestReceived(process_data);
						break;
					case ConnectionType::Http20:
						processed_length = OnHttp2RequestReceived(process_data);
						break;
					case ConnectionType::WebSocket:
						processed_length = OnWebSocketDataReceived(process_data);
						break;
					
					default:
						OV_ASSERT(false, "Unknown connection type");
						return;
				}
				
				// Error or Completed
				if (processed_length <= 0)
				{
					return;
				}

				process_data = process_data->Subdata(processed_length);
			}
		}

		ssize_t HttpConnection::OnHttp1RequestReceived(const std::shared_ptr<const ov::Data> &data)
		{
			// HTTP 1.1 only has one stream
			// Although HTTP/1.1 pipelining allows a client to send multiple requests at once, the server must respond in order.
			// So the server receives and processes requests one by one.

			if (_http_transaction == nullptr)
			{
				_http_transaction = std::make_shared<HttpTransaction>(GetSharedPtr());
			}

			auto processed_data_length = _http_transaction->OnRequestPacketReceived(data);
			if (processed_data_length <= 0)
			{
				// Error
				Close(PhysicalPortDisconnectReason::Error);
				return -1;
			}

			switch(_http_transaction->GetStatus())
			{
				case HttpTransaction::Status::Completed:
					if (_http_transaction->IsKeepAlive() == false)
					{
						Close(PhysicalPortDisconnectReason::Disconnect);
						return 0;
					}
					_http_transaction.reset();
					break;

				case HttpTransaction::Status::Upgrade:
					if (_http_transaction->IsWebSocketUpgradeRequest())
					{
						UpgradeToWebSocket(_http_transaction);
					}
					else
					{
						// TODO(h2) : Implement this
						OV_ASSERT2(false);
					}
					_http_transaction.reset();
					break;

				case HttpTransaction::Status::Moved:
					_http_transaction.reset();
					break;
					
				case HttpTransaction::Status::Error:
					Close(PhysicalPortDisconnectReason::Error);
					return -1;

				case HttpTransaction::Status::Init:
				case HttpTransaction::Status::Exchanging:
				default:
					break;
			}

			return processed_data_length;
		}

		ssize_t HttpConnection::OnWebSocketDataReceived(const std::shared_ptr<const ov::Data> &data)
		{
			

			if (_websocket_frame == nullptr)
			{
				_websocket_frame = std::make_shared<ws::Frame>();
			}
		
			ssize_t read_bytes = 0;
			if (_websocket_frame->Process(data, &read_bytes))
			{
				if (_websocket_session->OnFrameReceived(_websocket_frame) == false)
				{
					Close(PhysicalPortDisconnectReason::Error);
					return -1;
				}

				if (_websocket_session->GetStatus() == HttpTransaction::Status::Exchanging)
				{
					// Normal
				}
				else if (_websocket_session->GetStatus() == HttpTransaction::Status::Completed)
				{
					Close(PhysicalPortDisconnectReason::Disconnect);
					return -1;
				}
				else if (_websocket_session->GetStatus() == HttpTransaction::Status::Error)
				{
					Close(PhysicalPortDisconnectReason::Error);
					return -1;
				}
				else 
				{
					OV_ASSERT2(false);
					Close(PhysicalPortDisconnectReason::Error);
					return -1;
				}

				_websocket_frame.reset();
			}

			return read_bytes;
		}

		ssize_t HttpConnection::OnHttp2RequestReceived(const std::shared_ptr<const ov::Data> &data)
		{
			// HTTP 2.0에서는 _last_frame 으로 받아서 frame이 완성되면 Stream을 찾아서 넘겨야 한다. 헤더가 파싱되었으면 그 이후로 처리하면된다. 여기서는 frame을 길이로만 완성하고 어떤 frame인지 처리는 Stream 내에서 한다.)

			// HTTP1은 stream이 1개로 고정되어 있다. (현재 HttpTransaction이라는 이름임 : 수정할 것)
			
			// HTTP2 에서는 frame을 먼저 파싱한다. 

			// frame 정보로 stream을 찾는다.

			// stream에 frame을 넘긴다. 

			// frame을 다 받았다면 stream을 종료시킨다.

			return 0;
		}
	}  // namespace svr
}  // namespace http