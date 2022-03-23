//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_transaction.h"

#include "../../http_private.h"
#include "../http_connection.h"

namespace http
{
	namespace svr
	{
		// For HTTP/1.1
		HttpTransaction::HttpTransaction(const std::shared_ptr<HttpConnection> &connection)
			: _connection(connection)
		{
			_request = std::make_shared<HttpRequest>(connection->GetSocket());
			_response = std::make_shared<HttpResponse>(connection->GetSocket());

			// Set default connection type to request
			_request->SetConnectionType(ConnectionType::Http11);
			_request->SetTlsData(connection->GetTlsData());

			// Default Header for HTTP 1.1
			_response->SetTlsData(connection->GetTlsData());
			_response->SetHeader("Server", "OvenMediaEngine");
			_response->SetHeader("Content-Type", "text/html");
		}

		HttpTransaction::HttpTransaction(const std::shared_ptr<HttpTransaction> &transaction)
		{
			_request = transaction->_request;
			_response = transaction->_response;
			_status = transaction->_status;
			_connection = transaction->_connection;
			_extra = transaction->_extra;
			_keep_alive = transaction->_keep_alive;
		}

		// Get Status
		HttpTransaction::Status HttpTransaction::GetStatus() const
		{
			return _status;
		}

		// Set Status
		void HttpTransaction::SetStatus(HttpTransaction::Status status)
		{
			_status = status;
		}

		// Get Connection
		std::shared_ptr<HttpConnection> HttpTransaction::GetConnection() const
		{
			return _connection;
		}

		std::shared_ptr<HttpRequest> HttpTransaction::GetRequest()
		{
			return _request;
		}

		std::shared_ptr<HttpResponse> HttpTransaction::GetResponse()
		{
			return _response;
		}

		std::shared_ptr<const HttpRequest> HttpTransaction::GetRequest() const
		{
			return _request;
		}

		std::shared_ptr<const HttpResponse> HttpTransaction::GetResponse() const
		{
			return _response;
		}

		bool HttpTransaction::IsHttp2UpgradeRequest()
		{
			//TODO(h2) : Implement this
			return false;
		}

		bool HttpTransaction::IsWebSocketUpgradeRequest()
		{
			// RFC6455 - 4.2.1.  Reading the Client's Opening Handshake
			//
			// 1.   An HTTP/1.1 or higher GET request, including a "Request-URI"
			//      [RFC2616] that should be interpreted as a /resource name/
			//      defined in Section 3 (or an absolute HTTP/HTTPS URI containing
			//      the /resource name/).
			//
			// 2.   A |Host| header field containing the server's authority.
			//

			if ((_request->GetMethod() == Method::Get) && (_request->GetHttpVersionAsNumber() > 1.0))
			{
				if (
					// 3.   An |Upgrade| header field containing the value "websocket",
					//      treated as an ASCII case-insensitive value.
					(_request->GetHeader("UPGRADE").UpperCaseString().IndexOf("WEBSOCKET") >= 0L) &&

					// 4.   A |Connection| header field that includes the token "Upgrade",
					//      treated as an ASCII case-insensitive value.
					(_request->GetHeader("CONNECTION").UpperCaseString().IndexOf("UPGRADE") >= 0L) &&

					// 5.   A |Sec-WebSocket-Key| header field with a base64-encoded (see
					//      Section 4 of [RFC4648]) value that, when decoded, is 16 bytes in
					//      length.
					_request->IsHeaderExists("SEC-WEBSOCKET-KEY") &&

					// 6.   A |Sec-WebSocket-Version| header field, with a value of 13.
					(_request->GetHeader("SEC-WEBSOCKET-VERSION") == "13"))
				{
					// 7.   Optionally, an |Origin| header field.  This header field is sent
					//      by all browser clients.  A connection attempt lacking this
					//      header field SHOULD NOT be interpreted as coming from a browser
					//      client.
					//
					// 8.   Optionally, a |Sec-WebSocket-Protocol| header field, with a list
					//      of values indicating which protocols the client would like to
					//      speak, ordered by preference.
					//
					// 9.   Optionally, a |Sec-WebSocket-Extensions| header field, with a
					//      list of values indicating which extensions the client would like
					//      to speak.  The interpretation of this header field is discussed
					//      in Section 9.1.
					//
					// 10.  Optionally, other header fields, such as those used to send
					//      cookies or request authentication to a server.  Unknown header
					//      fields are ignored, as per [RFC2616].

					logtd("%s is websocket request", _request->ToString().CStr());

					return true;
				}
			}

			return false;
		}

		bool HttpTransaction::IsUpgradeRequest()
		{
			return IsWebSocketUpgradeRequest() || IsHttp2UpgradeRequest();
		}

		bool HttpTransaction::AcceptUpgrade()
		{
			if (IsWebSocketUpgradeRequest())
			{
				// RFC6455 - 4.2.2.  Sending the Server's Opening Handshake
				_response->SetStatusCode(StatusCode::SwitchingProtocols);

				_response->SetHeader("Upgrade", "websocket");
				_response->SetHeader("Connection", "Upgrade");

				// 4.  A |Sec-WebSocket-Accept| header field.  The value of this
				//    header field is constructed by concatenating /key/, defined
				//    above in step 4 in Section 4.2.2, with the string "258EAFA5-
				//    E914-47DA-95CA-C5AB0DC85B11", taking the SHA-1 hash of this
				//    concatenated value to obtain a 20-byte value and base64-
				//    encoding (see Section 4 of [RFC4648]) this 20-byte hash.
				const ov::String unique_id = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
				ov::String key = _request->GetHeader("SEC-WEBSOCKET-KEY");

				std::shared_ptr<ov::Data> hash = ov::MessageDigest::ComputeDigest(ov::CryptoAlgorithm::Sha1, (key + unique_id).ToData(false));
				ov::String base64 = ov::Base64::Encode(hash);

				_response->SetHeader("Sec-WebSocket-Accept", base64);

				// Send headers to client
				if (_response->Response() <= 0)
				{
					return false;
				}

				return true;
			}
			else if (IsHttp2UpgradeRequest())
			{
				return false;
			}

			return false;
		}

		// Get Connection Policy
		bool HttpTransaction::IsKeepAlive() const
		{
			return _keep_alive;
		}

		void HttpTransaction::SetConnectionPolicyByRequest()
		{
			if (_request->GetHttpVersionAsNumber() == 1.0)
			{
				_keep_alive = false;
			}
			else if (_request->GetHttpVersionAsNumber() == 1.1 || _request->GetHttpVersionAsNumber() == 2.0)
			{
				_keep_alive = true;
			}

			if (_request->GetHeader("Connection") == "Keep-Alive")
			{
				_keep_alive = true;
				_response->AddHeader("Connection", "Keep-Alive");
				_response->AddHeader("Keep-Alive", "timeout=5, max=100");
			}
			else if (_request->GetHeader("Connection") == "Close")
			{
				_keep_alive = false;
				_response->AddHeader("Connection", "Close");
			}
		}

		ssize_t HttpTransaction::OnRequestPacketReceived(const std::shared_ptr<const ov::Data> &data)
		{
			// Header parsing is complete and data is being received.
			if (_request->GetHeaderParingStatus() == StatusCode::OK)
			{
				auto interceptor = _connection->FindInterceptor(GetSharedPtr());
				
				// Here, payload data is passed to the interceptor rather than stored in the request. The interceptor may or may not store the payload in the request according to each role.
				auto comsumed_bytes = interceptor->OnDataReceived(GetSharedPtr(), data);
				if (comsumed_bytes <= 0)
				{
					SetStatus(Status::Error);
					return -1;
				}

				_received_data_size += comsumed_bytes;

				// TODO(getroot) : In the case of chunked transfer, there is no Content-length header, and it is necessary to parse the chunk to determine the end when the length is 0. Currently, chunked-transfer type request is not used, so it is not supported.

				if (_received_data_size >= _request->GetContentLength())
				{
					auto result = interceptor->OnRequestCompleted(GetSharedPtr());
					switch(result)
					{
						case InterceptorResult::Completed:
							SetStatus(Status::Completed);
							break;
						case InterceptorResult::Moved:
							SetStatus(Status::Moved);
							break;
						case InterceptorResult::Error:
						default:
							SetStatus(Status::Error);
							return -1;
					}
				}
				else
				{
					SetStatus(Status::Exchanging);
				}

				return comsumed_bytes;
			}
			// The header has not yet been parsed
			else if (_request->GetHeaderParingStatus() == StatusCode::PartialContent)
			{
				// Put more data to parse header
				auto comsumed_bytes = _request->AppendHeaderData(data);
				_received_header_size += comsumed_bytes;

				// Check if header parsing is complete
				if (_request->GetHeaderParingStatus() == StatusCode::PartialContent)
				{
					// Need more data to parse header
					return comsumed_bytes;
				}
				else if (_request->GetHeaderParingStatus() == StatusCode::OK)
				{
					// Header parsing is done
					logti("Client(%s) is requested uri: [%s]", _connection->GetSocket()->ToString().CStr(), _request->GetUri().CStr());

					if (IsUpgradeRequest() == true)
					{
						if (AcceptUpgrade() == false)
						{
							SetStatus(Status::Error);
							return -1;
						}
						
						SetStatus(Status::Upgrade);
						return comsumed_bytes;
					}

					SetConnectionPolicyByRequest();

					// Find interceptor using received header
					auto interceptor = _connection->FindInterceptor(GetSharedPtr());
					if (interceptor == nullptr)
					{
						SetStatus(Status::Error);
						_response->SetStatusCode(StatusCode::NotFound);
						_response->Response();
						return -1;
					}

					// Notify to interceptor
					auto need_to_disconnect = interceptor->OnRequestPrepared(GetSharedPtr());
					if (need_to_disconnect == false)
					{
						SetStatus(Status::Error);
						return -1;
					}

					// Check if the request is completed
					// If Content-length is 0, it means that the request is completed.
					if (_request->GetContentLength() == 0)
					{
						auto result = interceptor->OnRequestCompleted(GetSharedPtr());
						switch(result)
						{
							case InterceptorResult::Completed:
								SetStatus(Status::Completed);
								break;
							case InterceptorResult::Moved:
								SetStatus(Status::Moved);
								break;
							case InterceptorResult::Error:
							default:
								SetStatus(Status::Error);
								return -1;
						}
					}
					else
					{
						SetStatus(Status::Exchanging);
					}

					return comsumed_bytes;
				}
			}

			// Error
			logte("Invalid parse status: %d", _request->GetHeaderParingStatus());
			return -1;
		}
	}  // namespace svr
}  // namespace http
