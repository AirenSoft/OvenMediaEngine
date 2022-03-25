//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_exchange.h"

#include "../http_private.h"
#include "http_connection.h"

namespace http
{
	namespace svr
	{
		// For HTTP/1.1
		HttpExchange::HttpExchange(const std::shared_ptr<HttpConnection> &connection)
			: _connection(connection)
		{
			_request = CreateRequestInstance();
			_response = CreateResponseInstance();
		}

		HttpExchange::HttpExchange(const std::shared_ptr<HttpExchange> &exchange)
		{
			_request = exchange->_request;
			_response = exchange->_response;
			_status = exchange->_status;
			_connection = exchange->_connection;
			_extra = exchange->_extra;
			_keep_alive = exchange->_keep_alive;
		}

		std::shared_ptr<HttpRequest> HttpExchange::CreateRequestInstance()
		{
			return nullptr;
		}
		
		std::shared_ptr<HttpResponse> HttpExchange::CreateResponseInstance()
		{
			auto response = std::make_shared<HttpResponse>(_connection->GetSocket());
			response->SetTlsData(_connection->GetTlsData());
			response->SetHeader("Server", "OvenMediaEngine");
			response->SetHeader("Content-Type", "text/html");

			return response;
		}

		// Get Status
		HttpExchange::Status HttpExchange::GetStatus() const
		{
			return _status;
		}

		// Set Status
		void HttpExchange::SetStatus(HttpExchange::Status status)
		{
			_status = status;
		}

		// Get Connection
		std::shared_ptr<HttpConnection> HttpExchange::GetConnection() const
		{
			return _connection;
		}

		std::shared_ptr<HttpRequest> HttpExchange::GetRequest()
		{
			return _request;
		}

		std::shared_ptr<HttpResponse> HttpExchange::GetResponse()
		{
			return _response;
		}

		std::shared_ptr<const HttpRequest> HttpExchange::GetRequest() const
		{
			return _request;
		}

		std::shared_ptr<const HttpResponse> HttpExchange::GetResponse() const
		{
			return _response;
		}

		bool HttpExchange::IsHttp2UpgradeRequest()
		{
			//TODO(h2) : Implement this
			return false;
		}

		bool HttpExchange::IsWebSocketUpgradeRequest()
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

		bool HttpExchange::IsUpgradeRequest()
		{
			return IsWebSocketUpgradeRequest() || IsHttp2UpgradeRequest();
		}

		bool HttpExchange::AcceptUpgrade()
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
		bool HttpExchange::IsKeepAlive() const
		{
			return _keep_alive;
		}

		void HttpExchange::SetConnectionPolicyByRequest()
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
	}  // namespace svr
}  // namespace http
