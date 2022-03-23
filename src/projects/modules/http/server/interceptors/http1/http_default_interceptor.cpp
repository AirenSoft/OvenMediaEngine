//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_default_interceptor.h"

#include "../../../http_private.h"
#include "../../transactions/http_transaction.h"

// Currently, OME does not handle requests larger than 1 MB
#define MAX_HTTP_REQUEST_SIZE (1024LL * 1024LL)

namespace http
{
	namespace svr
	{
		DefaultInterceptor::DefaultInterceptor(const ov::String &pattern_prefix)
			: _pattern_prefix(pattern_prefix)
		{
		}

		bool DefaultInterceptor::Register(Method method, const ov::String &pattern, const RequestHandler &handler)
		{
			if (handler == nullptr)
			{
				return false;
			}

			ov::String whole_pattern;
			whole_pattern.Format("^%s%s$", _pattern_prefix.CStr(), pattern.CStr());

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
				logte("Invalid regex pattern: %s (Error: %s)", pattern.CStr(), error->What());
				return false;
			}

			return true;
		}

		bool DefaultInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpTransaction> &client)
		{
			// Process all requests because this is a default interceptor
			return true;
		}

		bool DefaultInterceptor::OnRequestPrepared(const std::shared_ptr<HttpTransaction> &transaction)
		{
			// Pre-allocate memory to process request body
			auto request = transaction->GetRequest();

			// TODO: Support for file upload & need to create a feature to block requests that are too large because too much CONTENT-LENGTH can cause OOM
			size_t content_length = request->GetContentLength();

			if (content_length > MAX_HTTP_REQUEST_SIZE)
			{
				return false;
			}

			if (content_length > 0L)
			{
				const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);

				if (request_body->Reserve(request->GetContentLength()) == false)
				{
					return false;
				}
			}

			return true;
		}

		ssize_t DefaultInterceptor::OnDataReceived(const std::shared_ptr<HttpTransaction> &transaction, const std::shared_ptr<const ov::Data> &data)
		{
			auto request = transaction->GetRequest();
			auto response = transaction->GetResponse();

			const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);
			size_t current_length = (request_body != nullptr) ? request_body->GetLength() : 0L;
			size_t content_length = request->GetContentLength();
			ssize_t consumed_length = 0;

			// request_body must be prepared if content length is greater than 0
			OV_ASSERT2((content_length == 0L) || ((content_length > 0L) && (request_body != nullptr)));

			std::shared_ptr<const ov::Data> process_data;
			if (current_length + data->GetLength() > content_length)
			{
				// HTTP/1.1 pipelining can contain more data than the content length. That's the next request.
				// logtw("Client sent too many data: expected: %ld, sent: %ld", content_length, (current_length + data->GetLength()));

				// The data sent by the client cannot exceed the content-length,
				// but if it exceeds the content-length, the data is processed only up to the content_length.
				process_data = data->Subdata(0L, content_length - current_length);
				consumed_length = content_length - current_length;
			}
			else
			{
				process_data = data;
				consumed_length = data->GetLength();
			}

			request_body->Append(process_data.get());
			return consumed_length;
		}

		InterceptorResult DefaultInterceptor::OnRequestCompleted(const std::shared_ptr<HttpTransaction> &transaction)
		{
			auto request = transaction->GetRequest();
			auto response = transaction->GetResponse();
			int handler_count = 0;

			// A variable to handle 403 Method not allowed
			bool regex_found = false;

			auto uri = ov::Url::Parse(request->GetUri());

			if (uri == nullptr)
			{
				logte("Could not parse uri: %s", request->GetUri().CStr());
				
				response->SetStatusCode(StatusCode::BadRequest);
				response->Response();

				return InterceptorResult::Error;
			}

			auto uri_target = uri->Path();

			for (auto &request_info : _request_handler_list)
			{
#if DEBUG
				logtd("Check if url [%s] is matches [%s]", uri_target.CStr(), request_info.pattern_string.CStr());
#endif	// DEBUG

				response->SetStatusCode(StatusCode::OK);

				auto matches = request_info.pattern.Matches(uri_target);
				auto error = matches.GetError();

				if (error == nullptr)
				{
#if DEBUG
					logtd("Matches: url [%s], pattern: [%s]", uri_target.CStr(), request_info.pattern_string.CStr());
#endif	// DEBUG

					regex_found = true;

					if (HTTP_CHECK_METHOD(request_info.method, request->GetMethod()))
					{
						handler_count++;

						request->SetMatchResult(matches);

						if (request_info.handler(transaction) == NextHandler::DoNotCall)
						{
							break;
						}

						// Call the next handler
					}
				}
				else
				{
#if DEBUG
					logtd("Not matched: url [%s], pattern: [%s] (with error: %s)", uri_target.CStr(), request_info.pattern_string.CStr(), error->What());
#endif	// DEBUG
				}
			}

			if (handler_count == 0)
			{
				if (regex_found)
				{
					// Handler matching the pattern was found, but no matching method was found
					response->SetStatusCode(StatusCode::MethodNotAllowed);
				}
				else
				{
					// Handler not found
					response->SetStatusCode(StatusCode::NotFound);
				}
			}
			else
			{
				// Everything is OK
			}

			response->Response();
			return InterceptorResult::Completed;
		}

		void DefaultInterceptor::OnError(const std::shared_ptr<HttpTransaction> &transaction, StatusCode status_code)
		{
			auto response = transaction->GetResponse();

			response->SetStatusCode(status_code);
		}

		void DefaultInterceptor::OnClosed(const std::shared_ptr<HttpConnection> &transaction, PhysicalPortDisconnectReason reason)
		{
			// Nothing to do
		}
	}  // namespace svr
}  // namespace http
