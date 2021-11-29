//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_connection.h"

#include "../http_private.h"
#include "http_server.h"

namespace http
{
	namespace svr
	{
		HttpConnection::HttpConnection(const std::shared_ptr<HttpServer> &server, std::shared_ptr<HttpRequest> &request, std::shared_ptr<HttpResponse> &response)
			: _server(std::move(server)),
			  _request(request),
			  _response(response)
		{
			OV_ASSERT2(_request != nullptr);
			OV_ASSERT2(_response != nullptr);
		}

		std::shared_ptr<HttpRequest> HttpConnection::GetRequest()
		{
			return _request;
		}

		std::shared_ptr<HttpResponse> HttpConnection::GetResponse()
		{
			return _response;
		}

		std::shared_ptr<const HttpRequest> HttpConnection::GetRequest() const
		{
			return _request;
		}

		std::shared_ptr<const HttpResponse> HttpConnection::GetResponse() const
		{
			return _response;
		}

		bool HttpConnection::IsWebSocketRequest()
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

		ssize_t HttpConnection::TryParseHeader(const std::shared_ptr<const ov::Data> &data)
		{
			auto &parser = _request->GetRequestParser();

			OV_ASSERT2(parser.GetParseStatus() == StatusCode::PartialContent);

			// Need to parse the header - Call ProcessData() to parse
			ssize_t processed_length = parser.ProcessData(data);

			switch (parser.GetParseStatus())
			{
				case StatusCode::OK:
					// Parsing is completed
					_request->PostProcess();
					break;

				case StatusCode::PartialContent:
					// Need more data - All datas must be consumed
					OV_ASSERT2((processed_length >= 0LL) && (static_cast<size_t>(processed_length) == data->GetLength()));
					break;

				default:
					// An error occurred
					OV_ASSERT2(processed_length == -1L);
					break;
			}

			return processed_length;
		}

		void HttpConnection::ProcessData(const std::shared_ptr<const ov::Data> &data)
		{
			auto client = GetSharedPtr();
			auto &parser = _request->GetRequestParser();

			bool need_to_disconnect = false;

			switch (parser.GetParseStatus())
			{
				case StatusCode::OK: {
					auto interceptor = _request->GetRequestInterceptor();

					if (interceptor != nullptr)
					{
						// If the request is parsed, bypass to the interceptor
						need_to_disconnect = (interceptor->OnHttpData(client, data) == InterceptorResult::Disconnect);
					}
					else
					{
						OV_ASSERT2(false);
						need_to_disconnect = true;
					}

					break;
				}

				case StatusCode::PartialContent: {
					// Need to parse HTTP header
					ssize_t processed_length = TryParseHeader(data);

					if (processed_length >= 0)
					{
						if (parser.GetParseStatus() == StatusCode::OK)
						{
							// Probe scheme
							_request->SetConnectionType(IsWebSocketRequest() ? RequestConnectionType::WebSocket : RequestConnectionType::HTTP);

							// Parsing is completed
							auto interceptor = _server->FindInterceptor(client);

							if (interceptor != nullptr)
							{
								_request->SetRequestInterceptor(interceptor);
							}
							else
							{
								logtw("No module could be found to handle this connection request : [%s]", _request->GetUri().CStr());
								// It will returns default interceptor
								interceptor = _request->GetRequestInterceptor();
							}

							if (_request->GetRequestInterceptor() == nullptr)
							{
								// The default interceptor must be set
								_response->SetStatusCode(StatusCode::InternalServerError);
								need_to_disconnect = true;
								OV_ASSERT2(false);
							}

							auto remote = _request->GetRemote();

							if (remote != nullptr)
							{
								logti("Client(%s) is requested uri: [%s]", remote->ToString().CStr(), _request->GetUri().CStr());
							}

							need_to_disconnect = need_to_disconnect || (interceptor->OnHttpPrepare(client) == InterceptorResult::Disconnect);
							need_to_disconnect = need_to_disconnect || (interceptor->OnHttpData(client, data->Subdata(processed_length)) == InterceptorResult::Disconnect);
						}
						else if (parser.GetParseStatus() == StatusCode::PartialContent)
						{
							// Need more data
						}
					}
					else
					{
						// An error occurred with the request
						_request->GetRequestInterceptor()->OnHttpError(client, StatusCode::BadRequest);
						need_to_disconnect = true;
					}

					break;
				}

				default:
					// If an error occurred during parse in the previous step, response was performed and Close() is called,
					// so if it is a normal situation, it should not enter here.
					logte("Invalid parse status: %d", parser.GetParseStatus());
					OV_ASSERT2(false);
					need_to_disconnect = true;
					break;
			}

			if (need_to_disconnect)
			{
				_response->Response();
				_response->Close();
			}
		}
	}  // namespace svr
}  // namespace http
