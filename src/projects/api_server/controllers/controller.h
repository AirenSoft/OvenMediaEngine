//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http_server/http_server.h>

#include <memory>

namespace api
{
	class ApiResponse
	{
	public:
		// Empty response (no response body with 200 OK)
		ApiResponse() = default;
		// Empty response (no response body with the status code)
		ApiResponse(HttpStatusCode status_code);
		// Used to send a JSON object with status code
		ApiResponse(HttpStatusCode status_code, const Json::Value &json);
		// Used to send a JSON object
		ApiResponse(const Json::Value &json);
		// Used to send an error
		ApiResponse(const std::shared_ptr<ov::Error> &error);

		bool SendToClient(const std::shared_ptr<HttpClient> &client);

	protected:
		HttpStatusCode _status_code = HttpStatusCode::OK;
		Json::Value _json = Json::Value::null;
	};

	template <typename Tclass>
	class Controller
	{
	public:
		void SetPrefix(const ov::String &prefix)
		{
			_prefix = prefix;
		}

		void SetInterceptor(const std::shared_ptr<HttpDefaultInterceptor> &interceptor)
		{
			_interceptor = interceptor;
		}

		template <typename Tcontroller>
		std::shared_ptr<Controller<Tcontroller>> CreateSubController(const ov::String &prefix_of_sub_controller)
		{
			auto instance = std::make_shared<Tcontroller>();

			instance->SetPrefix(_prefix + prefix_of_sub_controller);
			instance->SetInterceptor(_interceptor);

			instance->PrepareHandlers();

			return instance;
		}

		virtual void PrepareHandlers() = 0;

	protected:
		using ApiHandler = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client);

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandler &handler)
		{
			auto new_pattern = ov::String::FormatString("^%s%s$", _prefix.CStr(), pattern.CStr());
			auto that = dynamic_cast<Tclass *>(this);

			_interceptor->Register(method, new_pattern, [that, handler](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
				if (that != nullptr)
				{
					auto result = (that->*handler)(client);
					result.SendToClient(client);
				}
				else
				{
					OV_ASSERT2(false);
				}

				return HttpNextHandler::DoNotCall;
			});
		}

		// Register handlers
		void RegisterPost(const ov::String &pattern, const ApiHandler &handler)
		{
			Register(HttpMethod::Post, pattern, handler);
		}

		void RegisterGet(const ov::String &pattern, const ApiHandler &handler)
		{
			Register(HttpMethod::Get, pattern, handler);
		}

		void RegisterPut(const ov::String &pattern, const ApiHandler &handler)
		{
			Register(HttpMethod::Put, pattern, handler);
		}

		void RegisterDelete(const ov::String &pattern, const ApiHandler &handler)
		{
			Register(HttpMethod::Delete, pattern, handler);
		}

		// For all Handler registrations, prefix before pattern
		ov::String _prefix;
		std::shared_ptr<HttpDefaultInterceptor> _interceptor;
	};
}  // namespace api