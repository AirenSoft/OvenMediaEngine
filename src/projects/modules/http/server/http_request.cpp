//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_request.h"

#include <algorithm>

#include "../http_private.h"

namespace http
{
	namespace svr
	{
		HttpRequest::HttpRequest(const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<RequestInterceptor> &interceptor)
			: _client_socket(client_socket),
			  _interceptor(interceptor)
		{
			OV_ASSERT2(client_socket != nullptr);
		}

		std::shared_ptr<ov::ClientSocket> HttpRequest::GetRemote()
		{
			return _client_socket;
		}

		std::shared_ptr<const ov::ClientSocket> HttpRequest::GetRemote() const
		{
			return _client_socket;
		}

		void HttpRequest::SetTlsData(const std::shared_ptr<ov::TlsData> &tls_data)
		{
			_tls_data = tls_data;
			UpdateUri();
		}

		std::shared_ptr<ov::TlsData> HttpRequest::GetTlsData()
		{
			return _tls_data;
		}

		void HttpRequest::SetConnectionType(RequestConnectionType type)
		{
			_connection_type = type;
			UpdateUri();
		}

		RequestConnectionType HttpRequest::GetConnectionType() const
		{
			return _connection_type;
		}

		void HttpRequest::PostProcess()
		{
			UpdateUri();
		}

		ov::String HttpRequest::GetHeader(const ov::String &key) const noexcept
		{
			return _parser.GetHeader(key);
		}

		ov::String HttpRequest::GetHeader(const ov::String &key, ov::String default_value) const noexcept
		{
			return _parser.GetHeader(key, std::move(default_value));
		}

		const bool HttpRequest::IsHeaderExists(const ov::String &key) const noexcept
		{
			return _parser.IsHeaderExists(key);
		}

		void HttpRequest::UpdateUri()
		{
			auto host = GetHeader("Host");
			if (host.IsEmpty())
			{
				host = _client_socket->GetLocalAddress()->GetIpAddress();
			}

			ov::String scheme;
			if (GetConnectionType() == RequestConnectionType::HTTP)
			{
				scheme = "http";
			}
			else if (GetConnectionType() == RequestConnectionType::WebSocket)
			{
				scheme = "ws";
			}

			_request_uri = ov::String::FormatString("%s%s://%s%s", scheme.CStr(), (_tls_data != nullptr) ? "s" : "", host.CStr(), _parser.GetRequestTarget().CStr());
		}

		ov::String HttpRequest::ToString() const
		{
			return ov::String::FormatString("<HttpRequest: %p, Method: %d, uri: %s>", this, static_cast<int>(_parser.GetMethod()), GetUri().CStr());
		}
	}  // namespace svr
}  // namespace http
