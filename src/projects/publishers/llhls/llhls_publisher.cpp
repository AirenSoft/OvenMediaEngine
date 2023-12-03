//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "llhls_publisher.h"

#include <base/ovlibrary/url.h>

#include "llhls_private.h"
#include "llhls_session.h"

std::shared_ptr<LLHlsPublisher> LLHlsPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
{
	auto llhls = std::make_shared<LLHlsPublisher>(server_config, router);

	return llhls->Start() ? llhls : nullptr;
}

LLHlsPublisher::LLHlsPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
	: Publisher(server_config, router)
{
	logtd("LLHlsPublisher has been create");
}

LLHlsPublisher::~LLHlsPublisher()
{
	logtd("LLHlsPublisher has been terminated finally");
}

bool LLHlsPublisher::PrepareHttpServers(
	const std::vector<ov::String> &ip_list,
	const bool is_port_configured, const uint16_t port,
	const bool is_tls_port_configured, const uint16_t tls_port,
	const int worker_count)
{
	auto http_server_manager = http::svr::HttpServerManager::GetInstance();

	std::vector<std::shared_ptr<http::svr::HttpServer>> http_server_list;
	std::vector<std::shared_ptr<http::svr::HttpsServer>> https_server_list;

	if (http_server_manager->CreateServers(
			GetPublisherName(), "LLHLS",
			&http_server_list, &https_server_list,
			ip_list,
			is_port_configured, port,
			is_tls_port_configured, tls_port,
			nullptr, false,
			[&](const ov::SocketAddress &address, bool is_https, const std::shared_ptr<http::svr::HttpServer> &http_server) {
				http_server->AddInterceptor(CreateInterceptor());
			},
			worker_count))
	{
		std::lock_guard lock_guard{_http_server_list_mutex};
		_http_server_list = std::move(http_server_list);
		_https_server_list = std::move(https_server_list);

		return true;
	}

	http_server_manager->ReleaseServers(&http_server_list);
	http_server_manager->ReleaseServers(&https_server_list);

	return false;
}

bool LLHlsPublisher::Start()
{
	auto server_config = GetServerConfig();

	auto llhls_bind_config = server_config.GetBind().GetPublishers().GetLLHls();
	if (llhls_bind_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto llhls_module_config = server_config.GetModules().GetLLHls();
	if (llhls_module_config.IsEnabled() == false)
	{
		logtw("%s is disabled by configuration (Module is disabled)", GetPublisherName());
		return true;
	}

	bool is_configured = false;
	auto worker_count = llhls_bind_config.GetWorkerCount(&is_configured);
	worker_count = is_configured ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	bool is_port_configured;
	auto &port_config = llhls_bind_config.GetPort(&is_port_configured);

	bool is_tls_port_configured;
	auto &tls_port_config = llhls_bind_config.GetTlsPort(&is_tls_port_configured);

	if ((is_port_configured == false) && (is_tls_port_configured == false))
	{
		logtw("%s Server is disabled - No port is configured", GetPublisherName());
		return true;
	}

	return PrepareHttpServers(
			   server_config.GetIPList(),
			   is_port_configured, port_config.GetPort(),
			   is_tls_port_configured, tls_port_config.GetPort(),
			   worker_count) &&
		   Publisher::Start();
}

bool LLHlsPublisher::Stop()
{
	_http_server_list_mutex.lock();
	auto http_server_list = std::move(_http_server_list);
	auto https_server_list = std::move(_https_server_list);
	_http_server_list_mutex.unlock();

	auto http_server_manager = http::svr::HttpServerManager::GetInstance();

	http_server_manager->ReleaseServers(&http_server_list);
	http_server_manager->ReleaseServers(&https_server_list);

	return Publisher::Stop();
}

bool LLHlsPublisher::OnCreateHost(const info::Host &host_info)
{
	bool result = true;
	auto certificate = host_info.GetCertificate();

	if (certificate != nullptr)
	{
		std::lock_guard lock_guard{_http_server_list_mutex};

		for (auto &https_server : _https_server_list)
		{
			if (https_server->InsertCertificate(certificate) != nullptr)
			{
				result = false;
			}
		}
	}

	return result;
}

bool LLHlsPublisher::OnDeleteHost(const info::Host &host_info)
{
	bool result = true;
	auto certificate = host_info.GetCertificate();

	if (certificate != nullptr)
	{
		std::lock_guard lock_guard{_http_server_list_mutex};

		for (auto &https_server : _https_server_list)
		{
			if (https_server->RemoveCertificate(certificate) != nullptr)
			{
				result = false;
			}
		}
	}

	return result;
}

bool LLHlsPublisher::OnUpdateCertificate(const info::Host &host_info)
{
	bool result = true;
	auto certificate = host_info.GetCertificate();

	if (certificate != nullptr)
	{
		std::lock_guard lock_guard{_http_server_list_mutex};

		for (auto &https_server : _https_server_list)
		{
			if (https_server->InsertCertificate(certificate) != nullptr)
			{
				result = false;
			}
		}
	}

	return result;
}

std::shared_ptr<pub::Application> LLHlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return LLHlsApplication::Create(LLHlsPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool LLHlsPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto llhls_application = std::static_pointer_cast<LLHlsApplication>(application);
	if (llhls_application == nullptr)
	{
		logte("Could not found llhls application. app:%s", llhls_application->GetName().CStr());
		return false;
	}

	return true;
}

std::shared_ptr<LLHlsHttpInterceptor> LLHlsPublisher::CreateInterceptor()
{
	auto http_interceptor = std::make_shared<LLHlsHttpInterceptor>();

	// Register Request Handler
	http_interceptor->Register(http::Method::Options, R"((.+\.m3u8$)|(.+llhls\.m4s$))", [this](const std::shared_ptr<http::svr::HttpExchange> &exchange) -> http::svr::NextHandler {
		auto connection = exchange->GetConnection();
		auto request = exchange->GetRequest();
		auto response = exchange->GetResponse();

		auto request_url = request->GetParsedUri();
		if (request_url == nullptr)
		{
			logte("Could not parse request url: %s", request->GetUri().CStr());
			response->SetStatusCode(http::StatusCode::BadRequest);
			return http::svr::NextHandler::DoNotCall;
		}

		auto vhost_name = ocst::Orchestrator::GetInstance()->GetVhostNameFromDomain(request_url->Host());
		if (vhost_name.IsEmpty())
		{
			logte("Could not resolve vhost name from domain: %s", request_url->Host().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(request_url->Host(), request_url->App());
		if (vhost_app_name.IsValid() == false)
		{
			logte("Could not resolve application name from domain: %s", request_url->Host().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		auto application = std::static_pointer_cast<LLHlsApplication>(GetApplicationByName(vhost_app_name));
		if (application != nullptr)
		{
			application->GetCorsManager().SetupHttpCorsHeader(vhost_app_name, request, response, {http::Method::Options, http::Method::Get});
		}
		else
		{
			// CORS from default cors manager
			auto cors_manager_ref_opt = ocst::Orchestrator::GetInstance()->GetCorsManager(vhost_name);
			if (cors_manager_ref_opt.has_value())
			{
				const auto &cors_manager = cors_manager_ref_opt.value().get();
				cors_manager.SetupHttpCorsHeader(vhost_app_name, request, response);
			}
		}

		response->SetStatusCode(http::StatusCode::OK);
		response->SetHeader("Access-Control-Allow-Private-Network", "true");

		return http::svr::NextHandler::DoNotCall;
	});

	http_interceptor->Register(http::Method::Get, R"((.+\.m3u8$)|(.+llhls\.m4s$))", [this](const std::shared_ptr<http::svr::HttpExchange> &exchange) -> http::svr::NextHandler {
		auto connection = exchange->GetConnection();
		auto request = exchange->GetRequest();
		auto response = exchange->GetResponse();
		auto remote_address = request->GetRemote()->GetRemoteAddress();

		auto final_url = request->GetParsedUri();
		if (final_url == nullptr)
		{
			logte("Could not parse request url: %s", request->GetUri().CStr());
			response->SetStatusCode(http::StatusCode::BadRequest);
			return http::svr::NextHandler::DoNotCall;
		}

		// PORT can be omitted if port is default port, but SignedPolicy requires this information.
		if (final_url->Port() == 0)
		{
			final_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
		}
		auto requested_url = final_url;

		logtd("LLHLS requested(connection : %u): %s", connection->GetId(), request->GetUri().CStr());

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());

		auto host_name = final_url->Host();
		auto stream_name = final_url->Stream();

		uint64_t session_life_time = 0;
		bool access_control_enabled = IsAccessControlEnabled(final_url);

		// Check if the request is for the master playlist
		if (access_control_enabled == true && (final_url->File().IndexOf(".m3u8") > 0 && final_url->File().IndexOf("chunklist") == -1))
		{
			auto [signed_policy_result, signed_policy] = Publisher::VerifyBySignedPolicy(final_url, remote_address);
			if (signed_policy_result == AccessController::VerificationResult::Pass)
			{
				session_life_time = signed_policy->GetStreamExpireEpochMSec();
			}
			else if (signed_policy_result == AccessController::VerificationResult::Error)
			{
				logte("Could not resolve application name from domain: %s", final_url->Host().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
			else if (signed_policy_result == AccessController::VerificationResult::Fail)
			{
				logtw("%s", signed_policy->GetErrMessage().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}

			// Admission Webhooks
			auto request_info = std::make_shared<AccessController::RequestInfo>(final_url, remote_address, request->GetHeader("USER-AGENT"));

			auto [webhooks_result, admission_webhooks] = VerifyByAdmissionWebhooks(request_info);
			if (webhooks_result == AccessController::VerificationResult::Off)
			{
				// Success
			}
			else if (webhooks_result == AccessController::VerificationResult::Pass)
			{
				// Lifetime
				if (admission_webhooks->GetLifetime() != 0)
				{
					// Choice smaller value
					auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + admission_webhooks->GetLifetime();
					if (session_life_time == 0 || stream_expired_msec_from_webhooks < session_life_time)
					{
						session_life_time = stream_expired_msec_from_webhooks;
					}
				}

				// Redirect URL
				if (admission_webhooks->GetNewURL() != nullptr)
				{
					final_url = admission_webhooks->GetNewURL();
					if (final_url->Port() == 0)
					{
						final_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
					}

					vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());
					host_name = final_url->Host();
					stream_name = final_url->Stream();
				}
			}
			else if (webhooks_result == AccessController::VerificationResult::Error)
			{
				logtw("AdmissionWebhooks error : %s", final_url->ToUrlString().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
			else if (webhooks_result == AccessController::VerificationResult::Fail)
			{
				logtw("AdmissionWebhooks error : %s", admission_webhooks->GetErrReason().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
		}

		if (vhost_app_name.IsValid() == false)
		{
			logte("Could not resolve application name from domain: %s", final_url->Host().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		auto stream = std::static_pointer_cast<LLHlsStream>(GetStream(vhost_app_name, stream_name));
		if (stream == nullptr)
		{
			stream = std::dynamic_pointer_cast<LLHlsStream>(PullStream(final_url, vhost_app_name, host_name, stream_name));
			if (stream != nullptr)
			{
				logti("URL %s is requested", stream->GetMediaSource().CStr());
			}
			else
			{
				logte("Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
				response->SetStatusCode(http::StatusCode::NotFound);
				return http::svr::NextHandler::DoNotCall;
			}
		}

		// TODO(Getroot): Improve this so that the player's first request is played immediately. This policy was temporarily changed due to a performance issue at the edge.
		if (stream->WaitUntilStart(0) == false)
		{
			logtw("(%s/%s) stream has created but not started yet", vhost_app_name.CStr(), stream_name.CStr());
			response->SetStatusCode(http::StatusCode::Created);
			return http::svr::NextHandler::DoNotCall;
		}

		auto application = std::static_pointer_cast<LLHlsApplication>(stream->GetApplication());
		if (application == nullptr)
		{
			logte("Cannot find application (%s)", vhost_app_name.CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}
		auto origin_mode = application->IsOriginMode();

		std::shared_ptr<LLHlsSession> session = nullptr;

		// Master playlist (.m3u8 and NOT *chunklist*.m3u8)
		if (final_url->File().IndexOf(".m3u8") > 0 && final_url->File().IndexOf("chunklist") == -1)
		{
			session_id_t session_id = connection->GetId();

			try
			{
				// If this connection has been used by another session in the past, it is reused.
				session = std::any_cast<std::shared_ptr<LLHlsSession>>(connection->GetUserData(stream->GetUri()));
			}
			catch (const std::bad_any_cast &e)
			{
				session = std::static_pointer_cast<LLHlsSession>(stream->GetSession(session_id));
			}

			if (session == nullptr || session->GetStream() != stream)
			{
				// New HTTP Connection
				session = LLHlsSession::Create(session_id, origin_mode, "", stream->GetApplication(), stream, request->GetHeader("USER-AGENT"), session_life_time);
				if (session == nullptr)
				{
					logte("Could not create llhls session for request: %s", request->ToString().CStr());
					response->SetStatusCode(http::StatusCode::InternalServerError);
					return http::svr::NextHandler::DoNotCall;
				}
				session->SetRequestedUrl(requested_url);
				session->SetFinalUrl(final_url);

				stream->AddSession(session);
			}
		}
		// chunklist_x_x.m3u8?session=<session id>_<key>
		// x_x_x.m4s?session=<session id>_<key>
		else
		{
			session_id_t session_id = connection->GetId();
			ov::String session_key;

			if (origin_mode == false)
			{
				// ?session=<session id>_<key>
				// This collects them into one session even if one player connects through multiple connections.
				auto query_string = final_url->GetQueryValue("session");
				auto id_key = query_string.Split("_");
				if (id_key.size() != 2)
				{
					logte("Invalid session key : %s", final_url->ToUrlString().CStr());
					response->SetStatusCode(http::StatusCode::Unauthorized);
					return http::svr::NextHandler::DoNotCall;
				}

				session_id = ov::Converter::ToUInt32(id_key[0].CStr());
				session_key = id_key[1];
			}

			session = std::static_pointer_cast<LLHlsSession>(stream->GetSession(session_id));
			if (session == nullptr)
			{
				if (access_control_enabled == true)
				{
					logte("Invalid session_key : %s", final_url->ToUrlString().CStr());
					response->SetStatusCode(http::StatusCode::Unauthorized);
					return http::svr::NextHandler::DoNotCall;
				}
				else
				{
					// New HTTP Connection
					session = LLHlsSession::Create(session_id, origin_mode, session_key, stream->GetApplication(), stream, session_life_time);
					if (session == nullptr)
					{
						logte("Could not create llhls session for request: %s", request->ToString().CStr());
						response->SetStatusCode(http::StatusCode::InternalServerError);
						return http::svr::NextHandler::DoNotCall;
					}
					session->SetRequestedUrl(requested_url);
					session->SetFinalUrl(final_url);

					stream->AddSession(session);
				}
			}
			else
			{
				if (access_control_enabled == true && session_key != session->GetSessionKey())
				{
					logte("Invalid session_key : %s", final_url->ToUrlString().CStr());
					response->SetStatusCode(http::StatusCode::Unauthorized);
					return http::svr::NextHandler::DoNotCall;
				}
			}
		}

		// It will be used in CloseHandler
		connection->AddUserData(stream->GetUri(), session);
		session->UpdateLastRequest(connection->GetId());

		// Cors Setting
		application->GetCorsManager().SetupHttpCorsHeader(vhost_app_name, request, response);
		stream->SendMessage(session, std::make_any<std::shared_ptr<http::svr::HttpExchange>>(exchange));

		return http::svr::NextHandler::DoNotCallAndDoNotResponse;
	});

	// Set Close Handler
	http_interceptor->SetCloseHandler([this](const std::shared_ptr<http::svr::HttpConnection> &connection, PhysicalPortDisconnectReason reason) -> void {
		for (auto &user_data : connection->GetUserDataMap())
		{
			std::shared_ptr<LLHlsSession> session;

			try
			{
				session = std::any_cast<std::shared_ptr<LLHlsSession>>(user_data.second);
			}
			catch (const std::bad_any_cast &)
			{
				continue;
			}

			if (session != nullptr)
			{
				session->OnConnectionDisconnected(connection->GetId());
				if (session->IsNoConnection())
				{
					logtd("llhls session is closed : %u", session->GetId());
					// Remove session from stream
					auto stream = session->GetStream();
					if (stream != nullptr)
					{
						auto remote_address = connection->GetSocket()->GetRemoteAddress();
						auto requested_url = session->GetRequestedUrl();
						auto final_url = session->GetFinalUrl();
						if (remote_address && requested_url && final_url)
						{
							auto request_info = std::make_shared<AccessController::RequestInfo>(requested_url, remote_address, requested_url->ToUrlString(true) == final_url->ToUrlString(true) ? nullptr : final_url, session->GetUserAgent());

							SendCloseAdmissionWebhooks(request_info);
						}

						stream->RemoveSession(session->GetId());
					}
				}

				session->Stop();
			}
		}
	});

	return http_interceptor;
}