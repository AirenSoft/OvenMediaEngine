//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include <base/ovlibrary/url.h>

#include "llhls_publisher.h"
#include "llhls_session.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsPublisher> LLHlsPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto llhls = std::make_shared<LLHlsPublisher>(server_config, router);
	if (!llhls->Start())
	{
		return nullptr;
	}

	return llhls;
}

LLHlsPublisher::LLHlsPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
	logtd("LLHlsPublisher has been create");
}

LLHlsPublisher::~LLHlsPublisher()
{
	logtd("LLHlsPublisher has been terminated finally");
}

bool LLHlsPublisher::Start()
{
	auto server_config = GetServerConfig();

	auto llhls_module_config = server_config.GetModules().GetLLHls();
	if (llhls_module_config.IsEnabled() == false)
	{
		logtw("LLHls Module is disabled");
		return true;
	}

	auto llhls_bind_config = server_config.GetBind().GetPublishers().GetLLHls();

	if (llhls_bind_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_parsed = false;
	auto worker_count = llhls_bind_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	auto manager = http::svr::HttpServerManager::GetInstance();
	
	// Initialize HTTP Server (Non-TLS)
	bool http_server_result = true;
	auto port = llhls_bind_config.GetPort(&is_parsed);
	ov::SocketAddress address;
	if (is_parsed == true)
	{
		address = ov::SocketAddress(server_config.GetIp(), port.GetPort());

		_http_server = manager->CreateHttpServer("llhls", address, worker_count);
		if (_http_server != nullptr)
		{
			_http_server->AddInterceptor(CreateInterceptor());
		}
		else
		{
			logte("Could not initialize LLHLS http server : %s", address.ToString().CStr());
			http_server_result = false;
		}
	}

	// Initialze HTTPS Server
	bool https_server_result = true;
	auto &tls_port = llhls_bind_config.GetTlsPort(&is_parsed);
	ov::SocketAddress tls_address;
	if (http_server_result == true && is_parsed)
	{
		tls_address = ov::SocketAddress(server_config.GetIp(), tls_port.GetPort());

		_https_server = manager->CreateHttpsServer("llhls", tls_address, false, worker_count);
		if (_https_server != nullptr)
		{
			_https_server->AddInterceptor(CreateInterceptor());
		}
		else
		{
			logte("Could not initialize LLHLS https server : %s", tls_address.ToString().CStr());
			https_server_result = false;
		}
	}

	if (http_server_result == false ||  https_server_result == false)
	{
		manager->ReleaseServer(_http_server);
		manager->ReleaseServer(_https_server);
		return false;
	}

	logti("LLHLS Publisher is listening on %s%s%s%s", 
					port.GetPort() ? address.ToString().CStr() : "",
					tls_port.GetPort() ? ", " : "",
					tls_port.GetPort() ? "TLS: " : "",
					tls_port.GetPort() ? tls_address.ToString().CStr() : "");

	return Publisher::Start();
}

bool LLHlsPublisher::Stop()
{
	return Publisher::Stop();
}

bool LLHlsPublisher::OnCreateHost(const info::Host &host_info)
{
	if (_https_server != nullptr && host_info.GetCertificate() != nullptr)
	{
		return _https_server->AppendCertificate(host_info.GetCertificate()) == nullptr;
	}

	return true;
}

bool LLHlsPublisher::OnDeleteHost(const info::Host &host_info)
{
	if (_https_server != nullptr && host_info.GetCertificate() != nullptr)
	{
		return _https_server->RemoveCertificate(host_info.GetCertificate()) == nullptr;
	}

	return true;
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

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(request_url->Host(), request_url->App());
		if (vhost_app_name.IsValid() == false)
		{
			logte("Could not resolve application name from domain: %s", request_url->Host().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		response->SetStatusCode(http::StatusCode::OK);
		response->SetHeader("Content-Encoding", "gzip");
		response->SetHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
		response->SetHeader("Access-Control-Allow-Private-Network", "true");

		auto application = std::static_pointer_cast<LLHlsApplication>(GetApplicationByName(vhost_app_name));
		if(application == nullptr)
		{
			logte("Could not found application: %s", vhost_app_name.CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		application->GetCorsManager().SetupHttpCorsHeader(vhost_app_name, request, response);

		return http::svr::NextHandler::DoNotCall;
	});


	http_interceptor->Register(http::Method::Get, R"((.+\.m3u8$)|(.+llhls\.m4s$))", [this](const std::shared_ptr<http::svr::HttpExchange> &exchange) -> http::svr::NextHandler {

		auto connection = exchange->GetConnection();
		auto request = exchange->GetRequest();
		auto response = exchange->GetResponse();
		auto remote_address = request->GetRemote()->GetRemoteAddress();

		auto request_url = request->GetParsedUri();
		if (request_url == nullptr)
		{
			logte("Could not parse request url: %s", request->GetUri().CStr());
			response->SetStatusCode(http::StatusCode::BadRequest);
			return http::svr::NextHandler::DoNotCall;
		}

		// PORT can be omitted if port is default port, but SignedPolicy requires this information.
		if(request_url->Port() == 0)
		{
			request_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
		}

		logtd("LLHLS requested(connection : %u): %s", connection->GetId(), request->GetUri().CStr());

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(request_url->Host(), request_url->App());
		if (vhost_app_name.IsValid() == false)
		{
			logte("Could not resolve application name from domain: %s", request_url->Host().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		auto application = std::static_pointer_cast<LLHlsApplication>(GetApplicationByName(vhost_app_name));
		if (application == nullptr)
		{
			logte("Could not found application: %s", vhost_app_name.CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		auto origin_mode = application->IsOriginMode();
		auto host_name = request_url->Host();
		auto stream_name = request_url->Stream();

		uint64_t session_life_time = 0;
		bool access_control_enabled = IsAccessControlEnabled(request_url);

		// Check if the request is for the master playlist
		if (access_control_enabled == true && (request_url->File().IndexOf(".m3u8") > 0 && request_url->File().IndexOf("chunklist") == -1))
		{
			auto [signed_policy_result, signed_policy] =  Publisher::VerifyBySignedPolicy(request_url, remote_address);
			if(signed_policy_result == AccessController::VerificationResult::Pass)
			{
				session_life_time = signed_policy->GetStreamExpireEpochMSec();
			}
			else if(signed_policy_result == AccessController::VerificationResult::Error)
			{
				logte("Could not resolve application name from domain: %s", request_url->Host().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
			else if(signed_policy_result == AccessController::VerificationResult::Fail)
			{
				logtw("%s", signed_policy->GetErrMessage().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}

			// Admission Webhooks
			auto [webhooks_result, admission_webhooks] = VerifyByAdmissionWebhooks(request_url, remote_address);
			if(webhooks_result == AccessController::VerificationResult::Off)
			{
				// Success
			}
			else if(webhooks_result == AccessController::VerificationResult::Pass)
			{
				// Lifetime
				if(admission_webhooks->GetLifetime() != 0)
				{
					// Choice smaller value
					auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + admission_webhooks->GetLifetime();
					if(session_life_time == 0 || stream_expired_msec_from_webhooks < session_life_time)
					{
						session_life_time = stream_expired_msec_from_webhooks;
					}
				}

				// Redirect URL
				if(admission_webhooks->GetNewURL() != nullptr)
				{
					request_url = admission_webhooks->GetNewURL();
					if(request_url->Port() == 0)
					{
						request_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
					}

					vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(request_url->Host(), request_url->App());
					host_name = request_url->Host();
					stream_name = request_url->Stream();
				}
			}
			else if(webhooks_result == AccessController::VerificationResult::Error)
			{
				logtw("AdmissionWebhooks error : %s", request_url->ToUrlString().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
			else if(webhooks_result == AccessController::VerificationResult::Fail)
			{
				logtw("AdmissionWebhooks error : %s", admission_webhooks->GetErrReason().CStr());
				response->SetStatusCode(http::StatusCode::Unauthorized);
				return http::svr::NextHandler::DoNotCall;
			}
		}

		auto stream = application->GetStream(request_url->Stream());
		if (stream == nullptr)
		{
			// If the stream does not exists, request to the provider
			stream = PullStream(request_url, vhost_app_name, host_name, stream_name);
			if(stream == nullptr)
			{
				logte("Could not pull the stream : %s", request_url->Stream().CStr());
				response->SetStatusCode(http::StatusCode::NotFound);
				return http::svr::NextHandler::DoNotCall;
			}
		}

		if(stream->WaitUntilStart(10000) == false)
		{
			logtw("(%s/%s) stream has not started.", vhost_app_name.CStr(), stream_name.CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		std::shared_ptr<LLHlsSession> session = nullptr; 

		// Master playlist (.m3u8 and NOT *chunklist*.m3u8)
		if (request_url->File().IndexOf(".m3u8") > 0 && request_url->File().IndexOf("chunklist") == -1)
		{
			session_id_t session_id = connection->GetId();

			try
			{
				// If this connection has been used by another session in the past, it is reused.
				session = std::any_cast<std::shared_ptr<LLHlsSession>>(connection->GetUserData(stream->GetUri()));
			}
			catch (const std::bad_any_cast& e)
			{
				session = std::static_pointer_cast<LLHlsSession>(stream->GetSession(session_id));
			}

			if (session == nullptr || session->GetStream() != stream)
			{
				// New HTTP Connection
				session = LLHlsSession::Create(session_id, origin_mode, "", stream->GetApplication(), stream, session_life_time);
				if (session == nullptr)
				{
					logte("Could not create llhls session for request: %s", request->ToString().CStr());
					response->SetStatusCode(http::StatusCode::InternalServerError);
					return http::svr::NextHandler::DoNotCall;
				}

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
				auto query_string = request_url->GetQueryValue("session");
				auto id_key = query_string.Split("_");
				if (id_key.size() != 2)
				{
					logte("Invalid session key : %s", request_url->ToUrlString().CStr());
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
					logte("Invalid session_key : %s", request_url->ToUrlString().CStr());
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

					stream->AddSession(session);
				}
			}
			else
			{
				if (access_control_enabled == true && session_key != session->GetSessionKey())
				{
					logte("Invalid session_key : %s", request_url->ToUrlString().CStr());
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
						stream->RemoveSession(session->GetId());
					}
				}

				session->Stop();
			}
		}
	});

	return http_interceptor;
}