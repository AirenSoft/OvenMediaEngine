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
#include <monitoring/monitoring.h>

#include <memory>

#include "../helpers/helpers.h"
#include "../converters/converters.h"

namespace api
{
	class ApiResponse
	{
	public:
		// Empty response body ({}) with 200 OK
		ApiResponse() = default;
		// If status_code indicates 2xx then an empty response ({}), if not 2xx, sends an error message
		ApiResponse(HttpStatusCode status_code);
		// Used to send a JSON object with status code
		ApiResponse(HttpStatusCode status_code, const Json::Value &json);
		// Used to send a JSON object
		ApiResponse(const Json::Value &json);
		// Used to send an error
		ApiResponse(const std::shared_ptr<ov::Error> &error);

		// Copy ctor
		ApiResponse(const ApiResponse &response);
		// Move ctor
		ApiResponse(ApiResponse &&response);

		bool IsSucceeded()
		{
			// 2xx
			return (static_cast<int>(_status_code) / 100) == 2;
		}

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
		using Handler = std::function<void(Tclass *clazz, const std::shared_ptr<HttpClient> &client)>;

		// API Handlers
		using ApiHandler = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client);
		using ApiHandlerWithVHost = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<mon::HostMetrics> &vhost);
		using ApiHandlerWithVHostApp = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app);
		using ApiHandlerWithVHostAppStream = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app, const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);

		using ApiHandlerWithBody = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body);
		using ApiHandlerWithBodyVHost = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost);
		using ApiHandlerWithBodyVHostApp = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app);
		using ApiHandlerWithBodyVHostAppStream = ApiResponse (Tclass::*)(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app, const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);

#define API_CONTROLLER_GET_INIT()                                                 \
	[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult(); \
	[[maybe_unused]] std::shared_ptr<ov::Error> error;

#define API_CONTROLLER_GET_REQUEST_BODY()                              \
	ov::JsonObject json_object;                                        \
	error = json_object.Parse(client->GetRequest()->GetRequestBody()); \
	Json::Value request_body;                                          \
	if (error == nullptr)                                              \
	{                                                                  \
		request_body = json_object.GetJsonValue();                     \
	}

#define API_CONTROLLER_GET_VHOST()                             \
	std::shared_ptr<mon::HostMetrics> vhost;                   \
	std::string_view vhost_name;                               \
	if (error == nullptr)                                      \
	{                                                          \
		vhost_name = match_result.GetNamedGroup("vhost_name"); \
		vhost = GetVirtualHost(vhost_name);                    \
                                                               \
		if (vhost == nullptr)                                  \
		{                                                      \
			error = ov::Error::CreateError(                    \
				HttpStatusCode::NotFound,                      \
				"Could not find the virtual host: [%.*s]",     \
				vhost_name.length(), vhost_name.data());       \
		}                                                      \
	}

#define API_CONTROLLER_GET_APP()                               \
	std::shared_ptr<mon::ApplicationMetrics> app;              \
	std::string_view app_name;                                 \
	if (error == nullptr)                                      \
	{                                                          \
		app_name = match_result.GetNamedGroup("app_name");     \
		app = GetApplication(vhost, app_name);                 \
                                                               \
		if (app == nullptr)                                    \
		{                                                      \
			error = ov::Error::CreateError(                    \
				HttpStatusCode::NotFound,                      \
				"Could not find the application: [%.*s/%.*s]", \
				vhost_name.length(), vhost_name.data(),        \
				app_name.length(), app_name.data());           \
		}                                                      \
	}

#define API_CONTROLLER_GET_STREAM()                                  \
	std::shared_ptr<mon::StreamMetrics> stream;                      \
	std::string_view stream_name;                                    \
	std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams; \
	if (error == nullptr)                                            \
	{                                                                \
		stream_name = match_result.GetNamedGroup("stream_name");     \
		stream = GetStream(app, stream_name, &output_streams);       \
                                                                     \
		if (stream == nullptr)                                       \
		{                                                            \
			error = ov::Error::CreateError(                          \
				HttpStatusCode::NotFound,                            \
				"Could not find the stream: [%.*s/%.*s/%.*s]",       \
				vhost_name.length(), vhost_name.data(),              \
				app_name.length(), app_name.data(),                  \
				stream_name.length(), stream_name.data());           \
		}                                                            \
	}

#define API_CONTROLLER_REGISTER_HANDLERS(postfix)                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandler &handler)                   \
	{                                                                                              \
		Register(HttpMethod::postfix, pattern, handler);                                           \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHost &handler)          \
	{                                                                                              \
		Register(HttpMethod::postfix, pattern, handler);                                           \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHostApp &handler)       \
	{                                                                                              \
		Register(HttpMethod::postfix, pattern, handler);                                           \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHostAppStream &handler) \
	{                                                                                              \
		Register(HttpMethod::postfix, pattern, handler);                                           \
	}

#define API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(postfix)                                            \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBody &handler)               \
	{                                                                                                  \
		Register(HttpMethod::postfix, pattern, handler);                                               \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHost &handler)          \
	{                                                                                                  \
		Register(HttpMethod::postfix, pattern, handler);                                               \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHostApp &handler)       \
	{                                                                                                  \
		Register(HttpMethod::postfix, pattern, handler);                                               \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHostAppStream &handler) \
	{                                                                                                  \
		Register(HttpMethod::postfix, pattern, handler);                                               \
	}

		void Register(HttpMethod method, const ov::String &pattern, const Handler &handler)
		{
			auto new_pattern = ov::String::FormatString("^%s%s$", _prefix.CStr(), pattern.CStr());
			auto that = dynamic_cast<Tclass *>(this);

			_interceptor->Register(method, new_pattern, [that, handler](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
				if (that != nullptr)
				{
					handler(that, client);
				}
				else
				{
					OV_ASSERT2(false);
				}

				return HttpNextHandler::DoNotCall;
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandler &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithVHost &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_VHOST();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, vhost) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithVHostApp &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_VHOST();
				API_CONTROLLER_GET_APP();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, vhost, app) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithVHostAppStream &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_VHOST();
				API_CONTROLLER_GET_APP();
				API_CONTROLLER_GET_STREAM();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, vhost, app, stream, output_streams) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithBody &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_REQUEST_BODY();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, request_body) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithBodyVHost &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_REQUEST_BODY();
				API_CONTROLLER_GET_VHOST();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, request_body, vhost) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithBodyVHostApp &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_REQUEST_BODY();
				API_CONTROLLER_GET_VHOST();
				API_CONTROLLER_GET_APP();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, request_body, vhost, app) : error;
				result.SendToClient(client);
			});
		}

		void Register(HttpMethod method, const ov::String &pattern, const ApiHandlerWithBodyVHostAppStream &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<HttpClient> &client) {
				API_CONTROLLER_GET_INIT();
				API_CONTROLLER_GET_REQUEST_BODY();
				API_CONTROLLER_GET_VHOST();
				API_CONTROLLER_GET_APP();
				API_CONTROLLER_GET_STREAM();

				ApiResponse result = (error == nullptr) ? (clazz->*handler)(client, request_body, vhost, app, stream, output_streams) : error;
				result.SendToClient(client);
			});
		}

		// Register handlers
		API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(Post)
		API_CONTROLLER_REGISTER_HANDLERS(Get)
		API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(Put)
		API_CONTROLLER_REGISTER_HANDLERS(Delete)

		// For all Handler registrations, prefix before pattern
		ov::String _prefix;
		std::shared_ptr<HttpDefaultInterceptor> _interceptor;
	};
}  // namespace api