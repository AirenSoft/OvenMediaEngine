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
			
		}

		HttpExchange::HttpExchange(const std::shared_ptr<HttpExchange> &exchange)
		{
			_status = exchange->_status;
			_connection = exchange->_connection;
			_extra = exchange->_extra;
			_keep_alive = exchange->_keep_alive;
		}

		ov::String HttpExchange::ToString() const
		{
			return GetConnection()->ToString();
		}

		void HttpExchange::SetKeepAlive(bool keep_alive)
		{
			_keep_alive = keep_alive;
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

			if ((GetRequest()->GetMethod() == Method::Get) && (GetRequest()->GetHttpVersionAsNumber() > 1.0))
			{
				if (
					// 3.   An |Upgrade| header field containing the value "websocket",
					//      treated as an ASCII case-insensitive value.
					(GetRequest()->GetHeader("UPGRADE").UpperCaseString().IndexOf("WEBSOCKET") >= 0L) &&

					// 4.   A |Connection| header field that includes the token "Upgrade",
					//      treated as an ASCII case-insensitive value.
					(GetRequest()->GetHeader("CONNECTION").UpperCaseString().IndexOf("UPGRADE") >= 0L) &&

					// 5.   A |Sec-WebSocket-Key| header field with a base64-encoded (see
					//      Section 4 of [RFC4648]) value that, when decoded, is 16 bytes in
					//      length.
					GetRequest()->IsHeaderExists("SEC-WEBSOCKET-KEY") &&

					// 6.   A |Sec-WebSocket-Version| header field, with a value of 13.
					(GetRequest()->GetHeader("SEC-WEBSOCKET-VERSION") == "13"))
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

					logtd("%s is websocket request", GetRequest()->ToString().CStr());

					return true;
				}
			}

			return false;
		}

		bool HttpExchange::AcceptWebSocketUpgrade()
		{
			// RFC6455 - 4.2.2.  Sending the Server's Opening Handshake
			GetResponse()->SetStatusCode(StatusCode::SwitchingProtocols);

			GetResponse()->SetHeader("Upgrade", "websocket");
			GetResponse()->SetHeader("Connection", "Upgrade");

			// 4.  A |Sec-WebSocket-Accept| header field.  The value of this
			//    header field is constructed by concatenating /key/, defined
			//    above in step 4 in Section 4.2.2, with the string "258EAFA5-
			//    E914-47DA-95CA-C5AB0DC85B11", taking the SHA-1 hash of this
			//    concatenated value to obtain a 20-byte value and base64-
			//    encoding (see Section 4 of [RFC4648]) this 20-byte hash.
			const ov::String unique_id = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			ov::String key = GetRequest()->GetHeader("SEC-WEBSOCKET-KEY");

			std::shared_ptr<ov::Data> hash = ov::MessageDigest::ComputeDigest(ov::CryptoAlgorithm::Sha1, (key + unique_id).ToData(false));
			ov::String base64 = ov::Base64::Encode(hash);

			GetResponse()->SetHeader("Sec-WebSocket-Accept", base64);

			// Send headers to client
			if (GetResponse()->Response() <= 0)
			{
				return false;
			}

			return true;
		}

		// Get Connection Policy
		bool HttpExchange::IsKeepAlive() const
		{
			return _keep_alive;
		}

		void HttpExchange::SetConnectionPolicyByRequest()
		{
			if (GetRequest()->GetHttpVersionAsNumber() == 1.0)
			{
				_keep_alive = false;
			}
			else if (GetRequest()->GetHttpVersionAsNumber() == 1.1 || GetRequest()->GetHttpVersionAsNumber() == 2.0)
			{
				_keep_alive = true;
			}

			if (GetRequest()->GetHeader("Connection") == "Keep-Alive")
			{
				_keep_alive = true;
				GetResponse()->AddHeader("Connection", "Keep-Alive");
				GetResponse()->AddHeader("Keep-Alive", "timeout=5, max=100");
			}
			else if (GetRequest()->GetHeader("Connection") == "Close")
			{
				_keep_alive = false;
				GetResponse()->AddHeader("Connection", "Close");
			}
		}

		bool HttpExchange::OnRequestPrepared()
		{
			// Find interceptor using received header
			auto interceptor = GetConnection()->FindInterceptor(GetSharedPtr());
			if (interceptor == nullptr)
			{
				logtd("Interceptor is nullptr");
				SetStatus(Status::Error);
				GetResponse()->SetStatusCode(StatusCode::InternalServerError);
				GetResponse()->Response();
				return false;
			}

			// Call interceptor
			return interceptor->OnRequestPrepared(GetSharedPtr());
		}

		bool HttpExchange::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
		{
			auto interceptor = GetConnection()->FindInterceptor(GetSharedPtr());
			if (interceptor == nullptr)
			{
				logtd("Interceptor is nullptr");
				SetStatus(Status::Error);
				GetResponse()->SetStatusCode(StatusCode::InternalServerError);
				GetResponse()->Response();
				return false;
			}

			return interceptor->OnDataReceived(GetSharedPtr(), data);
		}

		InterceptorResult HttpExchange::OnRequestCompleted()
		{
			auto interceptor = GetConnection()->FindInterceptor(GetSharedPtr());
			if (interceptor == nullptr)
			{
				logtd("Interceptor is nullptr");
				SetStatus(Status::Error);
				GetResponse()->SetStatusCode(StatusCode::InternalServerError);
				GetResponse()->Response();
				return InterceptorResult::Error;
			}

			return interceptor->OnRequestCompleted(GetSharedPtr());
		}

	}  // namespace svr
}  // namespace http
