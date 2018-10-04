//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_default_interceptor.h"

#include "../../http_private.h"
#include "../../http_request.h"
#include "../../http_response.h"

bool HttpDefaultInterceptor::Register(HttpMethod method, const ov::String &pattern, const HttpRequestHandler &handler)
{
	if(handler == nullptr)
	{
		return false;
	}

	_request_handler_list.push_back((RequestInfo){
#if DEBUG
		.pattern_string = pattern,
#endif // DEBUG
		.pattern = std::regex(pattern),
		.method = method,
		.handler = handler
	});

	return true;
}

bool HttpDefaultInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response)
{
	// 기본 handler 이므로, 모든 request에 대해 무조건 처리
	return true;
}

void HttpDefaultInterceptor::OnHttpPrepare(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response)
{
	// request body를 처리하기 위해 메모리를 미리 할당해놓음
	// TODO: content-length가 너무 크면 비정상 종료 될 수 있으므로, 파일 업로드 지원 & 너무 큰 요청은 차단하는 기능 만들어야 함
	if(request->GetContentLength() > 0L)
	{
		const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);

		request_body->Reserve(request->GetContentLength());
	}
}

void HttpDefaultInterceptor::OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data)
{
	const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);
	ssize_t current_length = (request_body != nullptr) ? request_body->GetLength() : 0L;
	ssize_t content_length = request->GetContentLength();

	// content length가 0 초과면, request_body는 반드시 초기화 되어 있어야 함
	OV_ASSERT2((content_length == 0L) || ((content_length > 0L) && (request_body != nullptr)));

	std::shared_ptr<const ov::Data> process_data;
	if((current_length + data->GetLength()) > content_length)
	{
		logtw("Client sent too many data: expected: %ld, sent: %ld", content_length, (current_length + data->GetLength()));
		// 원래는, 클라이언트가 보낸 데이터는 content-length를 넘어설 수 없으나,
		// 만약에라도 넘어섰다면 data를 content_length까지만 처리함

		if(content_length - current_length > 0L)
		{
			process_data = data->Subdata(0L, content_length - current_length);
		}
		else
		{
			// content_length만큼 다 처리 한 상태

			// 정상적인 시나리오 에서는, 여기로 진입하면 안됨
			OV_ASSERT2(false);
			process_data = nullptr;
		}
	}
	else
	{
		process_data = data;
	}

	if(process_data != nullptr)
	{
		// request body에 데이터를 추가한 뒤
		request_body->Append(process_data.get());

		// 다 받아졌는지 확인
		if(request_body->GetLength() == content_length)
		{
			// 데이터가 다 받아졌다면, Register()된 handler 호출
			logtd("HTTP message is parsed successfully");

			// 처리할 수 있는 handler 찾음
			for(auto &request_info : _request_handler_list)
			{
#if DEBUG
				logtd("Check if url [%s] is matches [%s]", request->GetRequestTarget().CStr(), request_info.pattern_string.CStr());
#endif // DEBUG

				int handler_count = 0;
				// 403 Method not allowed 처리 하기 위한 수단
				bool regex_found = false;

				response->SetStatusCode(HttpStatusCode::OK);

				if(std::regex_match(request->GetRequestTarget().CStr(), request_info.pattern))
				{
					// 일단 패턴에 일치하는 handler 찾음
					regex_found = true;

					// method가 일치하는지 확인
					if(HTTP_CHECK_METHOD(request_info.method, request->GetMethod()))
					{
						handler_count++;

						request_info.handler(request, response);
					}
				}

				if(handler_count == 0)
				{
					if(regex_found)
					{
						// 패턴에 일치하는 handler는 찾았으나, 실제로 1의 handler도 실행이 안되었다면 Method not allowed임
						response->SetStatusCode(HttpStatusCode::MethodNotAllowed);
					}
					else
					{
						// URL을 처리할 수 있는 handler를 아예 찾을 수 없음
						response->SetStatusCode(HttpStatusCode::NotFound);
					}

					if(_error_handler != nullptr)
					{
						_error_handler(request, response);
					}
				}
				else
				{
					// 처리 완료
				}
			}
		}
		else
		{
			// 클라이언트가 아직 데이터를 덜 보냄
		}
	}
	else
	{
		// content-length만큼 다 처리 한 상태
	}

	response->Response();
	response->Close();
}

void HttpDefaultInterceptor::OnHttpError(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, HttpStatusCode status_code)
{
	response->SetStatusCode(status_code);
	response->Response();
	response->Close();
}

void HttpDefaultInterceptor::OnHttpClosed(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response)
{
	// 아무 처리 하지 않아도 됨
}
