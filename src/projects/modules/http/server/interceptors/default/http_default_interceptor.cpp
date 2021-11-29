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
#include "../../http_connection.h"

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
				logte("Invalid regex pattern: %s (Error: %s)", pattern.CStr(), error->ToString().CStr());
				return false;
			}

			return true;
		}

		bool DefaultInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpConnection> &client)
		{
			// Process all requests because this is a default interceptor
			return true;
		}

		InterceptorResult DefaultInterceptor::OnHttpPrepare(const std::shared_ptr<HttpConnection> &client)
		{
			// Pre-allocate memory to process request body
			auto request = client->GetRequest();

			// TODO: Support for file upload & need to create a feature to block requests that are too large because too much CONTENT-LENGTH can cause OOM
			size_t content_length = request->GetContentLength();

			if (content_length > (1024LL * 1024LL))
			{
				// Currently, OME does not handle requests larger than 1 MB
				return InterceptorResult::Disconnect;
			}

			if (content_length > 0L)
			{
				const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);

				if (request_body->Reserve(request->GetContentLength()) == false)
				{
					return InterceptorResult::Disconnect;
				}
			}

			return InterceptorResult::Keep;
		}

		InterceptorResult DefaultInterceptor::OnHttpData(const std::shared_ptr<HttpConnection> &client, const std::shared_ptr<const ov::Data> &data)
		{
			auto request = client->GetRequest();
			auto response = client->GetResponse();

			const std::shared_ptr<ov::Data> &request_body = GetRequestBody(request);
			size_t current_length = (request_body != nullptr) ? request_body->GetLength() : 0L;
			size_t content_length = request->GetContentLength();

			// request_body must be prepared if content length is greater than 0
			OV_ASSERT2((content_length == 0L) || ((content_length > 0L) && (request_body != nullptr)));

			std::shared_ptr<const ov::Data> process_data;
			if ((content_length > 0) && ((current_length + data->GetLength()) > content_length))
			{
				logtw("Client sent too many data: expected: %ld, sent: %ld", content_length, (current_length + data->GetLength()));

				// The data sent by the client cannot exceed the content-length,
				// but if it exceeds the content-length, the data is processed only up to the content_length.
				if (content_length > current_length)
				{
					process_data = data->Subdata(0L, content_length - current_length);
				}
				else
				{
					// Data has been processed as much as content_length, so it should not enter here
					OV_ASSERT2(false);
					return InterceptorResult::Disconnect;
				}
			}
			else
			{
				process_data = data;
			}

			if (process_data != nullptr)
			{
				request_body->Append(process_data.get());

				if (request_body->GetLength() >= content_length)
				{
					logtd("HTTP message is parsed successfully");

					int handler_count = 0;

					// A variable to handle 403 Method not allowed
					bool regex_found = false;

					auto uri = ov::Url::Parse(request->GetUri());

					if (uri == nullptr)
					{
						logte("Could not parse uri: %s", request->GetUri().CStr());
						return InterceptorResult::Disconnect;
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

								if (request_info.handler(client) == NextHandler::DoNotCall)
								{
									break;
								}

								// Call the next handler
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
				}
				else
				{
					// Need more data
					return InterceptorResult::Keep;
				}
			}
			else
			{
				// All data are processed
			}

			return InterceptorResult::Disconnect;
		}

		void DefaultInterceptor::OnHttpError(const std::shared_ptr<HttpConnection> &client, StatusCode status_code)
		{
			auto response = client->GetResponse();

			response->SetStatusCode(status_code);
		}

		void DefaultInterceptor::OnHttpClosed(const std::shared_ptr<HttpConnection> &client, PhysicalPortDisconnectReason reason)
		{
			// Nothing to do
		}
	}  // namespace svr
}  // namespace http
