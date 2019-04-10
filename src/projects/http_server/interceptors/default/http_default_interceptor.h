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
#include "http_server/interceptors/http_request_interceptor.h"

#include <regex>

/// HTTP 기본 처리기
class HttpDefaultInterceptor : public HttpRequestInterceptor
{
public:
	HttpDefaultInterceptor() = default;
	~HttpDefaultInterceptor() = default;

	// ECMAScript regex (http://www.cplusplus.com/reference/regex/ECMAScript)
	// method + pattern을 처리하는 handler 등록
	bool Register(HttpMethod method, 
			const ov::String &pattern, 
			const HttpRequestHandler &handler, 
			bool is_pattern_check = true);

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;
	bool OnHttpPrepare(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;
	bool OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;

protected:
	struct RequestInfo
	{
#if DEBUG
		ov::String pattern_string;
#endif // DEBUG
		std::regex pattern;
		bool is_pattern_check;
		HttpMethod method;
		HttpRequestHandler handler;
	};

	std::vector<RequestInfo> _request_handler_list;
};

