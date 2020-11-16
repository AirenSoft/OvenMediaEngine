//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_default_interceptor.h"

#include "../../http_client.h"
#include "../../http_private.h"

HttpDefaultInterceptor::HttpDefaultInterceptor(const ov::String &pattern_prefix)
	: _pattern_prefix(pattern_prefix)
{
}

bool HttpDefaultInterceptor::Register(HttpMethod method, const ov::String &pattern, const HttpRequestHandler &handler)
{
	if (handler == nullptr)
	{
		return false;
	}

	ov::String whole_pattern;
	whole_pattern.Format("%s%s", _pattern_prefix.CStr(), pattern.CStr());

	auto regex = ov::Regex(whole_pattern);
	auto error = regex.Compile();

	if (error == nullptr)
	{
		_request_handler_list.push_back((RequestInfo) {
#if DEBUG
			.pattern_string = whole_pattern,
#endif	// DEBUG
			.pattern = std::move(regex),
			.method = method,
			.handler = handler
		});
	}
	else
	{
		logte("Invalid regex pattern: %s (Error: %s)", pattern.CStr(), error->ToString().CStr());
		return false;
	}

	return true;
}

bool HttpDefaultInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpClient> &client)
{
	// 기본 handler 이므로, 모든 request에 대해 무조건 처리
	return true;
}

HttpInterceptorResult HttpDefaultInterceptor::OnHttpPrepare(const std::shared_ptr<HttpClient> &client)
{
	// request body를 처리하기 위해 메모리를 미리 할당해놓음
	auto request = client->GetRequest();

	// TODO: content-length가 너무 크면 비정상 종료 될 수 있으므로, 파일 업로드 지원 & 너무 큰 요청은 차단하는 기능 만들어야 함
	ssize_t content_length = request->GetContentLength();

	if (content_length > (1024LL * 1024LL))
	{
		// Currently, OME does not handle requests larger than 1 MB
		return HttpInterceptorResult::Disconnect;
	}

	if (content_length > 0L)
	{
		const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);

		if (request_body->Reserve(static_cast<size_t>(request->GetContentLength())) == false)
		{
			return HttpInterceptorResult::Disconnect;
		}
	}

	return HttpInterceptorResult::Keep;
}

HttpInterceptorResult HttpDefaultInterceptor::OnHttpData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data)
{
	auto request = client->GetRequest();
	auto response = client->GetResponse();

	const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);
	ssize_t current_length = (request_body != nullptr) ? request_body->GetLength() : 0L;
	ssize_t content_length = request->GetContentLength();

	// content length가 0 초과면, request_body는 반드시 초기화 되어 있어야 함
	OV_ASSERT2((content_length == 0L) || ((content_length > 0L) && (request_body != nullptr)));

	std::shared_ptr<const ov::Data> process_data;
	if ((content_length > 0) && ((current_length + static_cast<ssize_t>(data->GetLength())) > content_length))
	{
		logtw("Client sent too many data: expected: %ld, sent: %ld", content_length, (current_length + data->GetLength()));
		// 원래는, 클라이언트가 보낸 데이터는 content-length를 넘어설 수 없으나,
		// 만약에라도 넘어섰다면 data를 content_length까지만 처리함

		if (content_length > current_length)
		{
			process_data = data->Subdata(0L, static_cast<size_t>(content_length - current_length));
		}
		else
		{
			// content_length만큼 다 처리 한 상태

			// 정상적인 시나리오 에서는, 여기로 진입하면 안됨
			OV_ASSERT2(false);
			return HttpInterceptorResult::Disconnect;
		}
	}
	else
	{
		process_data = data;
	}

	if (process_data != nullptr)
	{
		// request body에 데이터를 추가한 뒤
		request_body->Append(process_data.get());

		// 다 받아졌는지 확인
		if (static_cast<ssize_t>(request_body->GetLength()) >= content_length)
		{
			// 데이터가 다 받아졌다면, Register()된 handler 호출
			logtd("HTTP message is parsed successfully");

			// 처리할 수 있는 handler 찾음
			int handler_count = 0;

			// 403 Method not allowed 처리 하기 위한 수단
			bool regex_found = false;

			auto uri = ov::Url::Parse(request->GetUri());

			if(uri == nullptr)
			{
				logte("Could not parse uri: %s", request->GetUri().CStr());
				return HttpInterceptorResult::Disconnect;
			}

			auto uri_target = uri->Path();

			for (auto &request_info : _request_handler_list)
			{
#if DEBUG
				logtd("Check if url [%s] is matches [%s]", uri_target.CStr(), request_info.pattern_string.CStr());
#endif	// DEBUG

				response->SetStatusCode(HttpStatusCode::OK);

				auto matches = request_info.pattern.Matches(uri_target);
				auto &error = matches.GetError();

				if (error == nullptr)
				{
#if DEBUG
					logtd("Matches: url [%s], pattern: [%s]", uri_target.CStr(), request_info.pattern_string.CStr());
#endif	// DEBUG

					// 일단 패턴에 일치하는 handler 찾음
					regex_found = true;

					// method가 일치하는지 확인
					if (HTTP_CHECK_METHOD(request_info.method, request->GetMethod()))
					{
						handler_count++;

						request->SetMatchResult(matches);

						if (request_info.handler(client) == HttpNextHandler::DoNotCall)
						{
							break;
						}
						else
						{
							// Call the next handler
						}
					}
				}
				else
				{
#if DEBUG
					logtd("Not matched: url [%s], pattern: [%s] (with error: %s)", uri_target.CStr(), request_info.pattern_string.CStr(), error->ToString().CStr());
#endif	// DEBUG
				}
			}

			if (handler_count == 0)
			{
				if (regex_found)
				{
					// 패턴에 일치하는 handler는 찾았으나, 실제로 handler가 실행이 안되었다면 Method not allowed임
					response->SetStatusCode(HttpStatusCode::MethodNotAllowed);
				}
				else
				{
					// URL을 처리할 수 있는 handler를 아예 찾을 수 없음
					response->SetStatusCode(HttpStatusCode::NotFound);
				}
			}
			else
			{
				// 처리 완료
			}
		}
		else
		{
			// 클라이언트가 아직 데이터를 덜 보냄

			// 데이터를 더 기다려야 함
			return HttpInterceptorResult::Keep;
		}
	}
	else
	{
		// content-length만큼 다 처리 한 상태
	}

	return HttpInterceptorResult::Disconnect;
}

void HttpDefaultInterceptor::OnHttpError(const std::shared_ptr<HttpClient> &client, HttpStatusCode status_code)
{
	auto response = client->GetResponse();

	response->SetStatusCode(status_code);
}

void HttpDefaultInterceptor::OnHttpClosed(const std::shared_ptr<HttpClient> &client, PhysicalPortDisconnectReason reason)
{
	// 아무 처리 하지 않아도 됨
}
