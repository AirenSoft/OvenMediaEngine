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
	bool Register(HttpMethod method, const ov::String &pattern, const HttpRequestHandler &handler);

	// request를 처리하다 오류가 발생할 수 있는데, 이럴 때 호출될 callback
	bool RegisterErrorHandler(HttpRequestErrorHandler error_callback)
	{
		_error_handler = std::move(error_callback);
		return true;
	}

	//--------------------------------------------------------------------
	// Implementation of HttpRequestInterceptorInterface
	//--------------------------------------------------------------------
	bool IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response) override;
	void OnHttpPrepare(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;
	void OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data) override;
	void OnHttpError(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, HttpStatusCode status_code) override;
	void OnHttpClosed(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) override;

protected:
	struct RequestInfo
	{
#if DEBUG
		ov::String pattern_string;
#endif // DEBUG
		std::regex pattern;
		HttpMethod method;
		HttpRequestHandler handler;
	};

	// 오류 발생 시 호출할 handler
	HttpRequestErrorHandler _error_handler;

	std::vector<RequestInfo> _request_handler_list;
};

