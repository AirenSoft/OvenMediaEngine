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

#include "../../http_datastructure.h"
#include "../http_request_interceptor.h"

/// Default HTTP processor
class HttpDefaultInterceptor : public HttpRequestInterceptor
{
public:
	HttpDefaultInterceptor() = default;
	HttpDefaultInterceptor(const ov::String &pattern_prefix);

	// Register handler to handle method and pattern
	// pattern consists of ECMAScript regex (http://www.cplusplus.com/reference/regex/ECMAScript)
	bool Register(HttpMethod method, const ov::String &pattern, const HttpRequestHandler &handler);

	bool RegisterGet(const ov::String &pattern, const HttpRequestHandler &handler)
	{
		return Register(HttpMethod::Get, pattern, handler);
	}

	bool RegisterPost(const ov::String &pattern, const HttpRequestHandler &handler)
	{
		return Register(HttpMethod::Post, pattern, handler);
	}

	bool RegisterPut(const ov::String &pattern, const HttpRequestHandler &handler)
	{
		return Register(HttpMethod::Put, pattern, handler);
	}

	bool RegisterDelete(const ov::String &pattern, const HttpRequestHandler &handler)
	{
		return Register(HttpMethod::Delete, pattern, handler);
	}

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	HttpRequestConnectionType GetConnectionType() override
	{
		return HttpRequestConnectionType::HTTP;
	}

	bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) override;
	HttpInterceptorResult OnHttpPrepare(const std::shared_ptr<HttpClient> &client) override;
	HttpInterceptorResult OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpClient> &client, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpClient> &client, PhysicalPortDisconnectReason reason) override;

protected:
	struct RequestInfo
	{
#if DEBUG
		ov::String pattern_string;
#endif	// DEBUG
		ov::Regex pattern;
		HttpMethod method;
		HttpRequestHandler handler;
	};

	ov::String _pattern_prefix;

	std::vector<RequestInfo> _request_handler_list;
};
