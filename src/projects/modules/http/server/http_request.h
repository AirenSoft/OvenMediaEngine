//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../protocol/http1/http_request_parser.h"
#include "http_request_interceptor.h"

namespace http
{
	namespace svr
	{
		class HttpExchange;

		class HttpRequest : public ov::EnableSharedFromThis<HttpRequest>
		{
		public:
			friend class RequestInterceptor;

			HttpRequest(const std::shared_ptr<ov::ClientSocket> &client_socket);
			~HttpRequest() override = default;

			std::shared_ptr<ov::ClientSocket> GetRemote();
			std::shared_ptr<const ov::ClientSocket> GetRemote() const;

			void SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data);
			std::shared_ptr<ov::TlsServerData> GetTlsData();

			void SetConnectionType(ConnectionType type);
			ConnectionType GetConnectionType() const;

			ssize_t AppendHeaderData(const std::shared_ptr<const ov::Data> &data)
			{
				if (GetHeaderParingStatus() == StatusCode::OK)
				{
					// Already parsed
					return 0;
				}
				else if (GetHeaderParingStatus() == StatusCode::PartialContent)
				{
					auto consumed_bytes = _http_header_parser.AppendData(data);
					if (GetHeaderParingStatus() == StatusCode::OK)
					{
						PostProcess();
					}
					return consumed_bytes;
				}
				else
				{
					// Error
					return -1;
				}
			}

			StatusCode GetHeaderParingStatus() const
			{
				return _http_header_parser.GetStatus();
			}

			Method GetMethod() const noexcept
			{
				return _http_header_parser.GetMethod();
			}

			ov::String GetHttpVersion() const noexcept
			{
				return _http_header_parser.GetHttpVersion();
			}

			double GetHttpVersionAsNumber() const noexcept
			{
				return _http_header_parser.GetHttpVersionAsNumber();
			}

			// Full URI (including domain and port)
			// Example: http://<domain>:<port>/<app>/<stream>/...
			const ov::String &GetUri() const noexcept
			{
				return _request_uri;
			}

			// Path of the URI (including query strings & excluding domain and port)
			// Example: /<app>/<stream>/...?a=b&c=d
			const ov::String &GetRequestTarget() const noexcept
			{
				return _http_header_parser.GetRequestTarget();
			}

			/// HTTP body 데이터 길이 반환
			///
			/// @return body 데이터 길이. 파싱이 제대로 되지 않았거나, request header에 명시가 안되어 있으면 0이 반환됨.
			size_t GetContentLength() const noexcept
			{
				return _http_header_parser.GetContentLength();
			}

			std::shared_ptr<const ov::Data> GetRequestBody() const
			{
				return _request_body;
			}

			const std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> &GetRequestHeader() const noexcept
			{
				return _http_header_parser.GetHeaders();
			}

			ov::String GetHeader(const ov::String &key) const noexcept;
			ov::String GetHeader(const ov::String &key, ov::String default_value) const noexcept;
			const bool IsHeaderExists(const ov::String &key) const noexcept;

			void SetMatchResult(ov::MatchResult match_result)
			{
				_match_result = std::move(match_result);
			}

			const ov::MatchResult &GetMatchResult() const
			{
				return _match_result;
			}

			// Received server name using SNI
			ov::String GetServerName() const
			{
				if (_tls_data != nullptr)
				{
					return _tls_data->GetTls().GetServerName();
				}

				return "";
			}

			ov::String ToString() const;

		protected:
			// HttpRequestInterceptorInterface를 통해, 다른 interceptor에서 사용됨
			const std::shared_ptr<ov::Data> &GetRequestBodyInternal()
			{
				if (_request_body == nullptr)
				{
					_request_body = std::make_shared<ov::Data>();
				}

				return _request_body;
			}

			void PostProcess();
			void UpdateUri();

			std::shared_ptr<ov::ClientSocket> _client_socket;
			ConnectionType _connection_type = ConnectionType::Unknown;
			std::shared_ptr<ov::TlsServerData> _tls_data;

			ov::MatchResult _match_result;
			ov::String _request_uri;

			// For HTTP/1.1
			prot::h1::HttpRequestHeaderParser _http_header_parser;

			// TODO(h2) : Implement this
			// For HTTP/2.0
			// Http2RequestParser _parser;

			// HTTP body
			std::shared_ptr<ov::Data> _request_body;
		};
	}  // namespace svr
}  // namespace http
