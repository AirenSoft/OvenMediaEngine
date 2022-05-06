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

	auto port = llhls_bind_config.GetPort(&is_parsed);
	if (is_parsed == true)
	{
		logtw("LLHLS does not work on Non-TLS HTTP Ports. Ignores the <LLHLS><Port>%u</Port></LLHLS> setting.", port.GetPort());
	}

	// Initialze HTTPS Server
	auto manager = http::svr::HttpServerManager::GetInstance();
	auto &tls_port = llhls_bind_config.GetTlsPort(&is_parsed);
	if (is_parsed)
	{
		ov::SocketAddress tls_address = ov::SocketAddress(server_config.GetIp(), tls_port.GetPort());

		_https_server = manager->CreateHttpsServer("llhls", tls_address, false, worker_count);
		if (_https_server != nullptr)
		{
			_https_server->AddInterceptor(CreateInterceptor());
		}
		else
		{
			logte("Could not initialize LLHLS https server");
		}
	}
	else
	{
		logte("In LLHLS, TlsPort must be enabled since LLHLS works only over HTTP/2");
		return false;
	}

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

	http_interceptor->Register(http::Method::Get, R"(.+llhls\.(m3u8|m4s)$)", [this](const std::shared_ptr<http::svr::HttpExchange> &exchange) -> http::svr::NextHandler {

		auto request = exchange->GetRequest();
		auto response = exchange->GetResponse();

		logtd("LLHLS requested: %s", request->GetUri().CStr());

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

		auto stream = GetStream(vhost_app_name, request_url->Stream());
		if (stream == nullptr)
		{
			logte("Could not find stream: %s", request_url->Stream().CStr());
			response->SetStatusCode(http::StatusCode::NotFound);
			return http::svr::NextHandler::DoNotCall;
		}

		session_id_t session_id = exchange->GetConnection()->GetId();
		auto session = stream->GetSession(session_id);
		if (session == nullptr)
		{
			// New HTTP Connection
			session = LLHlsSession::Create(session_id, stream->GetApplication(), stream);
			if (session == nullptr)
			{
				logte("Could not create llhls session for request: %s", request->ToString().CStr());
				response->SetStatusCode(http::StatusCode::InternalServerError);
				return http::svr::NextHandler::DoNotCall;
			}

			stream->AddSession(session);
		}

		stream->SendMessage(session, std::make_any<std::shared_ptr<http::svr::HttpExchange>>(exchange));

		return http::svr::NextHandler::DoNotCallAndDoNotResponse;
	});

	return http_interceptor;
}