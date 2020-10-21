//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../http_datastructure.h"

#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <modules/physical_port/physical_port_observer.h>

enum class HttpRequestConnectionType
{
	HTTP = 0,
	WebSocket
};

class HttpRequest;
class HttpRequestInterceptor
{
public:
	virtual ~HttpRequestInterceptor() {}

	virtual HttpRequestConnectionType GetConnectionType() = 0;

	// request를 처리할 수 있는 interceptor인지 여부 반환
	// 만약, true를 반환하면 앞으로 이 interceptor만 호출됨
	virtual bool IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client) = 0;

	/// IsInterceptorForRequest() 직후, request/response를 초기화 하기 위해 호출됨
	///
	/// @param request HTTP 요청 정보
	/// @param response HTTP 응답을 처리하는 instance
	///
	/// @return false를 반환할 경우, client와의 연결을 종료함
	virtual HttpInterceptorResult OnHttpPrepare(const std::shared_ptr<HttpClient> &client) = 0;

	/// 클라이언트로 부터 데이터를 수신함
	///
	/// @param request HTTP 요청 정보
	/// @param response HTTP 응답을 처리하는 instance
	/// @param data 수신한 데이터
	///
	/// @return false를 반환할 경우, client와의 연결을 종료함
	virtual HttpInterceptorResult OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data) = 0;

	/// 처리 도중 오류 발생
	///
	/// @param request HTTP 요청 정보
	/// @param response HTTP 응답을 처리하는 instance
	/// @param status_code 오류 종류
	///
	/// @remark interceptor가 HttpServer의 default interceptor일 경우, OnHttpPrepare()가 호출되기 전에 호출될 수 있음
	virtual void OnHttpError(const std::shared_ptr<HttpClient> &client, HttpStatusCode status_code) = 0;

	/// 클라이언트에서 연결을 해제하였음
	///
	/// @param request HTTP 요청 정보
	/// @param response HTTP 응답을 처리하는 instance
	///
	/// @remark 연결이 이미 해제 된 상태이기 때문에, 이 상태에서는 더 이상 클라이언트로 응답을 보낼 수 없음.
	/// Closed는 Error가 발생해도 항상 호출되는 것을 보장함
	virtual void OnHttpClosed(const std::shared_ptr<HttpClient> &client, PhysicalPortDisconnectReason reason) = 0;

protected:
	static const std::shared_ptr<ov::Data> &GetRequestBody(const std::shared_ptr<HttpRequest> &request);
};
