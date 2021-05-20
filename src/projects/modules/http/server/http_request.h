//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../parser/http_request_parser.h"
#include "interceptors/http_request_interceptor.h"

namespace http
{
	namespace svr
	{
		class HttpConnection;

		class HttpRequest : public ov::EnableSharedFromThis<HttpRequest>
		{
		public:
			friend class RequestInterceptor;

			HttpRequest(const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<RequestInterceptor> &interceptor);
			~HttpRequest() override = default;

			std::shared_ptr<ov::ClientSocket> GetRemote();
			std::shared_ptr<const ov::ClientSocket> GetRemote() const;

			void SetTlsData(const std::shared_ptr<ov::TlsData> &tls_data);
			std::shared_ptr<ov::TlsData> GetTlsData();

			void SetConnectionType(RequestConnectionType type);
			RequestConnectionType GetConnectionType() const;

			HttpParser &GetRequestParser()
			{
				return _parser;
			}

			const HttpParser &GetRequestParser() const
			{
				return _parser;
			}

			void PostProcess();

			Method GetMethod() const noexcept
			{
				return _parser.GetMethod();
			}

			ov::String GetHttpVersion() const noexcept
			{
				return _parser.GetHttpVersion();
			}

			double GetHttpVersionAsNumber() const noexcept
			{
				return _parser.GetHttpVersionAsNumber();
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
				return _parser.GetRequestTarget();
			}

			/// HTTP body 데이터 길이 반환
			///
			/// @return body 데이터 길이. 파싱이 제대로 되지 않았거나, request header에 명시가 안되어 있으면 0이 반환됨.
			size_t GetContentLength() const noexcept
			{
				return _parser.GetContentLength();
			}

			std::shared_ptr<const ov::Data> GetRequestBody() const
			{
				return _request_body;
			}

			const std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> &GetRequestHeader() const noexcept
			{
				return _parser.GetHeaders();
			}

			ov::String GetHeader(const ov::String &key) const noexcept;
			ov::String GetHeader(const ov::String &key, ov::String default_value) const noexcept;
			const bool IsHeaderExists(const ov::String &key) const noexcept;

			bool SetRequestInterceptor(const std::shared_ptr<RequestInterceptor> &interceptor) noexcept
			{
				_interceptor = interceptor;
				return true;
			}

			void SetMatchResult(ov::MatchResult match_result)
			{
				_match_result = std::move(match_result);
			}

			const ov::MatchResult &GetMatchResult() const
			{
				return _match_result;
			}

			const std::shared_ptr<RequestInterceptor> &GetRequestInterceptor()
			{
				return _interceptor;
			}

			std::any GetExtra() const
			{
				return _extra;
			}

			template <typename T>
			std::shared_ptr<T> GetExtraAs() const
			{
				try
				{
					return std::any_cast<std::shared_ptr<T>>(_extra);
				}
				catch ([[maybe_unused]] const std::bad_any_cast &e)
				{
				}

				return nullptr;
			}

			template <typename T>
			void SetExtra(std::shared_ptr<T> extra)
			{
				_extra = std::move(extra);
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

			void UpdateUri();

			std::shared_ptr<ov::ClientSocket> _client_socket;
			RequestConnectionType _connection_type = RequestConnectionType::Unknown;
			std::shared_ptr<ov::TlsData> _tls_data;

			// request 처리를 담당하는 객체
			std::shared_ptr<RequestInterceptor> _interceptor;

			ov::MatchResult _match_result;

			ov::String _request_uri;

			HttpRequestParser _parser;

			// HTTP body
			std::shared_ptr<ov::Data> _request_body;

			std::any _extra;
		};
	}  // namespace svr
}  // namespace http
