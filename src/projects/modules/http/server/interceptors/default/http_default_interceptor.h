//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <regex>

#include "../../../http_datastructure.h"
#include "../http_request_interceptor.h"

namespace http
{
	namespace svr
	{
		/// Default HTTP processor
		class DefaultInterceptor : public RequestInterceptor
		{
		public:
			DefaultInterceptor() = default;
			DefaultInterceptor(const ov::String &pattern_prefix);

			// Register handler to handle method and pattern
			// pattern consists of ECMAScript regex (http://www.cplusplus.com/reference/regex/ECMAScript)
			bool Register(Method method, const ov::String &pattern, const RequestHandler &handler);

			bool RegisterGet(const ov::String &pattern, const RequestHandler &handler)
			{
				return Register(Method::Get, pattern, handler);
			}

			bool RegisterPost(const ov::String &pattern, const RequestHandler &handler)
			{
				return Register(Method::Post, pattern, handler);
			}

			bool RegisterPut(const ov::String &pattern, const RequestHandler &handler)
			{
				return Register(Method::Put, pattern, handler);
			}

			bool RegisterDelete(const ov::String &pattern, const RequestHandler &handler)
			{
				return Register(Method::Delete, pattern, handler);
			}

			//--------------------------------------------------------------------
			// Implementation of RequestInterceptor
			//--------------------------------------------------------------------
			RequestConnectionType GetConnectionType() override
			{
				return RequestConnectionType::HTTP;
			}

			bool IsInterceptorForRequest(const std::shared_ptr<const HttpConnection> &client) override;
			InterceptorResult OnHttpPrepare(const std::shared_ptr<HttpConnection> &client) override;
			InterceptorResult OnHttpData(const std::shared_ptr<HttpConnection> &client, const std::shared_ptr<const ov::Data> &data) override;
			void OnHttpError(const std::shared_ptr<HttpConnection> &client, StatusCode status_code) override;
			void OnHttpClosed(const std::shared_ptr<HttpConnection> &client, PhysicalPortDisconnectReason reason) override;

		protected:
			struct RequestInfo
			{
#if DEBUG
				ov::String pattern_string;
#endif	// DEBUG
				ov::Regex pattern;
				Method method;
				RequestHandler handler;
			};

			ov::String _pattern_prefix;

			std::vector<RequestInfo> _request_handler_list;
		};
	}  // namespace svr
}  // namespace http
