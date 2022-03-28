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

#include "../http_datastructure.h"
#include "http_request_interceptor.h"

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
			virtual bool IsInterceptorForRequest(const std::shared_ptr<const HttpExchange> &client) override;
			virtual bool OnRequestPrepared(const std::shared_ptr<HttpExchange> &exchange) override;
			virtual bool OnDataReceived(const std::shared_ptr<HttpExchange> &exchange, const std::shared_ptr<const ov::Data> &data) override;
			virtual InterceptorResult OnRequestCompleted(const std::shared_ptr<HttpExchange> &exchange) override;
			virtual void OnError(const std::shared_ptr<HttpExchange> &exchange, StatusCode status_code) override;
			virtual void OnClosed(const std::shared_ptr<HttpConnection> &stream, PhysicalPortDisconnectReason reason) override;

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
