//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../http_datastructure.h"
#include "../http_request_interceptor.h"

#include <regex>

/// HTTP 기본 처리기
class HttpDefaultInterceptor : public HttpRequestInterceptor
{
public:
	// ECMAScript regex (http://www.cplusplus.com/reference/regex/ECMAScript)
	// method + pattern을 처리하는 handler 등록
	bool Register(HttpMethod method, const ov::String &pattern, const HttpRequestHandler &handler);

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	HttpRequestConnectionType GetConnectionType() override {return HttpRequestConnectionType::HTTP;}
	
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) override;
	HttpInterceptorResult OnHttpPrepare(const std::shared_ptr<HttpClient> &client) override;
	HttpInterceptorResult OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpClient> &client, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpClient> &client) override;

protected:
	struct RequestInfo
	{
#if DEBUG
		ov::String pattern_string;
#endif  // DEBUG
		std::regex pattern;
		HttpMethod method;
		HttpRequestHandler handler;
	};

	std::vector<RequestInfo> _request_handler_list;
};
