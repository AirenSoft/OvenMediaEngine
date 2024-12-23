//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http/server/http_server.h>
#include <modules/json_serdes/converters.h>
#include <monitoring/monitoring.h>

#include <memory>

#include "../helpers/helpers.h"

namespace api
{
	class Server;

	class ApiResponse
	{
	public:
		// {
		//     "statusCode": 200,
		//     "message": "OK"
		// }
		ApiResponse();

		// {
		//     "statusCode": <status_code>,
		//     "message": <message_of_status_code>
		// }
		ApiResponse(http::StatusCode status_code);

		// {
		//     "statusCode": <status_code>,
		//     "message": <message_of_status_code>,
		//     "response": <json>
		// }
		ApiResponse(http::StatusCode status_code, const Json::Value &json);

		// {
		//     "statusCode": <status_code>,
		//     "message": <message_of_status_code>
		// }
		ApiResponse(MultipleStatus status_codes, const Json::Value &json);

		// {
		//     "statusCode": 200,
		//     "message": "OK",
		//     "response": <json>
		// }
		ApiResponse(const Json::Value &json);

		// {
		//     "statusCode": <error->GetCode()>,
		//     "message": <error->GetMessage()>
		// }
		ApiResponse(const std::exception *error);

		// Copy ctor
		ApiResponse(const ApiResponse &response);
		// Move ctor
		ApiResponse(ApiResponse &&response);

		bool IsSucceeded()
		{
			// 2xx
			return (static_cast<int>(_status_code) / 100) == 2;
		}

		bool SendToClient(const std::shared_ptr<http::svr::HttpExchange> &client);

	protected:
		void SetResponse(http::StatusCode status_code);
		void SetResponse(http::StatusCode status_code, const char *message);
		void SetResponse(http::StatusCode status_code, const char *message, const Json::Value &json);

		http::StatusCode _status_code = http::StatusCode::OK;
		Json::Value _json = Json::Value::null;
	};

	class ControllerInterface
	{
	};

	template <typename Tclass>
	class Controller : public ControllerInterface
	{
	public:
		virtual ~Controller() = default;

		void SetServer(const std::shared_ptr<Server> &server)
		{
			_server = server;
		}

		void SetPrefix(const ov::String &prefix)
		{
			_prefix = prefix;
		}

		void SetInterceptor(const std::shared_ptr<http::svr::DefaultInterceptor> &interceptor)
		{
			_interceptor = interceptor;
		}

		template <typename Tcontroller>
		void CreateSubController(const ov::String &prefix_of_sub_controller)
		{
			auto instance = std::make_shared<Tcontroller>();

			instance->SetServer(_server);
			instance->SetPrefix(_prefix + prefix_of_sub_controller);
			instance->SetInterceptor(_interceptor);

			instance->PrepareHandlers();

			_sub_controllers.push_back(instance);
		}

		virtual void PrepareHandlers() = 0;

	protected:
		using Handler = std::function<void(Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client)>;

		// API Handlers
		using ApiHandler = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client);
		using ApiHandlerWithVHost = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const std::shared_ptr<mon::HostMetrics> &vhost);
		using ApiHandlerWithVHostApp = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app);
		using ApiHandlerWithVHostAppStream = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app, const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);

		using ApiHandlerWithBody = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body);
		using ApiHandlerWithBodyVHost = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost);
		using ApiHandlerWithBodyVHostApp = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app);
		using ApiHandlerWithBodyVHostAppStream = ApiResponse (Tclass::*)(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body, const std::shared_ptr<mon::HostMetrics> &vhost, const std::shared_ptr<mon::ApplicationMetrics> &app, const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);

	protected:
		void Register(http::Method method, const ov::String &pattern, const Handler &handler)
		{
			auto new_pattern = ov::String::FormatString("^%s%s$", _prefix.CStr(), pattern.CStr());
			auto that = dynamic_cast<Tclass *>(this);

			_interceptor->Register(method, new_pattern, [that, handler](const std::shared_ptr<http::svr::HttpExchange> &client) -> http::svr::NextHandler {
				if (that != nullptr)
				{
					try
					{
						handler(that, client);
					}
					catch (const http::HttpError &error)
					{
						logw("APIController", "HTTP error occurred: %s", error.What());
						ApiResponse(&error).SendToClient(client);
					}
					catch (const cfg::ConfigError &error)
					{
						logw("APIController", "Config error occurred: %s", error.GetDetailedMessage().CStr());
						ApiResponse(&error).SendToClient(client);
					}
					catch (const std::exception &error)
					{
						logw("APIController", "Unknown error occurred: %s", error.what());
						ApiResponse(&error).SendToClient(client);
					}
				}
				else
				{
					OV_ASSERT2(false);
				}

				return http::svr::NextHandler::DoNotCall;
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandler &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				ApiResponse result = (clazz->*handler)(client);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithVHost &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				ApiResponse result = (clazz->*handler)(client, vhost_metrics);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithVHostApp &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				std::shared_ptr<mon::ApplicationMetrics> app_metrics;
				GetApplicationMetrics(match_result, vhost_metrics, &app_metrics);

				ApiResponse result = (clazz->*handler)(client, vhost_metrics, app_metrics);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithVHostAppStream &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				std::shared_ptr<mon::ApplicationMetrics> app_metrics;
				GetApplicationMetrics(match_result, vhost_metrics, &app_metrics);

				std::shared_ptr<mon::StreamMetrics> stream_metrics;
				std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;
				GetStreamMetrics(match_result, vhost_metrics, app_metrics, &stream_metrics, &output_streams);

				ApiResponse result = (clazz->*handler)(client, vhost_metrics, app_metrics, stream_metrics, output_streams);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithBody &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				Json::Value request_body;
				GetRequestBody(client, &request_body);

				ApiResponse result = (clazz->*handler)(client, request_body);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithBodyVHost &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				Json::Value request_body;
				GetRequestBody(client, &request_body);

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				ApiResponse result = (clazz->*handler)(client, request_body, vhost_metrics);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithBodyVHostApp &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				Json::Value request_body;
				GetRequestBody(client, &request_body);

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				std::shared_ptr<mon::ApplicationMetrics> app_metrics;
				GetApplicationMetrics(match_result, vhost_metrics, &app_metrics);

				ApiResponse result = (clazz->*handler)(client, request_body, vhost_metrics, app_metrics);
				result.SendToClient(client);
			});
		}

		void Register(http::Method method, const ov::String &pattern, const ApiHandlerWithBodyVHostAppStream &handler)
		{
			Register(method, pattern, [handler](Tclass *clazz, const std::shared_ptr<http::svr::HttpExchange> &client) {
				[[maybe_unused]] auto &match_result = client->GetRequest()->GetMatchResult();

				Json::Value request_body;
				GetRequestBody(client, &request_body);

				std::shared_ptr<mon::HostMetrics> vhost_metrics;
				GetVirtualHostMetrics(match_result, &vhost_metrics);

				std::shared_ptr<mon::ApplicationMetrics> app_metrics;
				GetApplicationMetrics(match_result, vhost_metrics, &app_metrics);

				std::shared_ptr<mon::StreamMetrics> stream_metrics;
				std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;
				GetStreamMetrics(match_result, vhost_metrics, app_metrics, &stream_metrics, &output_streams);

				ApiResponse result = (clazz->*handler)(client, request_body, vhost_metrics, app_metrics, stream_metrics, output_streams);
				result.SendToClient(client);
			});
		}

		// Register handlers
#define API_CONTROLLER_REGISTER_HANDLERS(postfix)                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandler &handler)                   \
	{                                                                                              \
		Register(http::Method::postfix, pattern, handler);                                         \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHost &handler)          \
	{                                                                                              \
		Register(http::Method::postfix, pattern, handler);                                         \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHostApp &handler)       \
	{                                                                                              \
		Register(http::Method::postfix, pattern, handler);                                         \
	}                                                                                              \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithVHostAppStream &handler) \
	{                                                                                              \
		Register(http::Method::postfix, pattern, handler);                                         \
	}

#define API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(postfix)                                            \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBody &handler)               \
	{                                                                                                  \
		Register(http::Method::postfix, pattern, handler);                                             \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHost &handler)          \
	{                                                                                                  \
		Register(http::Method::postfix, pattern, handler);                                             \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHostApp &handler)       \
	{                                                                                                  \
		Register(http::Method::postfix, pattern, handler);                                             \
	}                                                                                                  \
	void Register##postfix(const ov::String &pattern, const ApiHandlerWithBodyVHostAppStream &handler) \
	{                                                                                                  \
		Register(http::Method::postfix, pattern, handler);                                             \
	}

		API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(Post)
		API_CONTROLLER_REGISTER_HANDLERS(Get)
		API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(Patch)
		API_CONTROLLER_REGISTER_HANDLERS_WITH_BODY(Put)
		API_CONTROLLER_REGISTER_HANDLERS(Delete)

	protected:
		// For all Handler registrations, prefix before pattern
		ov::String _prefix;
		std::shared_ptr<http::svr::DefaultInterceptor> _interceptor;

		// A variable to keep shared_ptr are alive
		std::vector<std::shared_ptr<ControllerInterface>> _sub_controllers;

		std::shared_ptr<Server> _server;
	};
}  // namespace api
