//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "web_console_private.h"
#include "web_console_server.h"

WebConsoleServer::WebConsoleServer(const info::Application &application_info, PrivateToken token)
	: _application_info(application_info)
{
}

std::shared_ptr<WebConsoleServer> WebConsoleServer::Create(const info::Application &application_info)
{
	auto instance = std::make_shared<WebConsoleServer>(application_info, (PrivateToken){});

	if(instance != nullptr)
	{
		const auto &web_console = application_info.GetWebConsole();
		instance->_web_console = web_console;

		auto host = web_console.GetParentAs<cfg::Host>("Host");
		auto address = ov::SocketAddress(host->GetIp(), static_cast<uint16_t>(web_console.GetListenPort()));
		const auto &tls = web_console.GetTls();

		auto certificate = tls.IsParsed() ? instance->GetCertificate(tls.GetCertPath(), tls.GetKeyPath()) : nullptr;
		auto chain_certificate = tls.IsParsed() ? instance->GetChainCertificate(tls.GetChainCertPath()) : nullptr;

		logti("Trying to start WebConsole on %s...", address.ToString().CStr());

		if(instance->Start(address, certificate, chain_certificate))
		{
			return instance;
		}
		else
		{
			logte("Could not start WebConsole");
		}
	}
	else
	{
		logte("Could not initialize WebConsole");
	}

	return nullptr;
}

bool WebConsoleServer::Start(const ov::SocketAddress &address, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate)
{
	if(_http_server != nullptr)
	{
		OV_ASSERT(false, "Server is already running");
		return false;
	}

	if(certificate != nullptr)
	{
		auto https_server = std::make_shared<HttpsServer>();

		https_server->SetLocalCertificate(certificate);
		https_server->SetChainCertificate(chain_certificate);

		_http_server = https_server;
	}
	else
	{
		_http_server = std::make_shared<HttpServer>();
	}

	return InitializeServer() && _http_server->Start(address);
}

bool WebConsoleServer::Stop()
{
	if(_http_server == nullptr)
	{
		return false;
	}

	if(_http_server->Stop())
	{
		_http_server = nullptr;

		return true;
	}

	return false;
}

std::shared_ptr<Certificate> WebConsoleServer::GetCertificate(ov::String cert_path, ov::String key_path)
{
	if(
		(cert_path.IsEmpty() == false) &&
		(key_path.IsEmpty() == false)
		)
	{
		auto certificate = std::make_shared<Certificate>();

		logti("Trying to create a certificate using files\n\tCert path: %s\n\tPrivate key path: %s",
		      cert_path.CStr(),
		      key_path.CStr()
		);

		auto error = certificate->GenerateFromPem(cert_path, key_path);

		if(error == nullptr)
		{
			return certificate;
		}

		logte("Could not create a certificate from files: %s", error->ToString().CStr());
	}
	else
	{
		// TLS is disabled
	}

	return nullptr;
}

std::shared_ptr<Certificate> WebConsoleServer::GetChainCertificate(ov::String chain_cert_path)
{
	if(
		(chain_cert_path.IsEmpty() == false)
		)
	{
		auto certificate = std::make_shared<Certificate>();

		logti("Trying to create a chain certificate using file: %s",
		      chain_cert_path.CStr()
		);

		auto error = certificate->GenerateFromPem(chain_cert_path, true);

		if(error == nullptr)
		{
			return certificate;
		}

		logte("Could not create a chain certificate from file: %s", error->ToString().CStr());
	}
	else
	{
		// TLS is disabled
	}

	return nullptr;
}

bool WebConsoleServer::InitializeServer()
{
	auto http_interceptor = std::make_shared<HttpDefaultInterceptor>();
	ov::String document_root = ov::PathManager::GetCanonicalPath(_web_console.GetDocumentPath());

	http_interceptor->Register(HttpMethod::Post, "/api/login", [this](const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) -> void
	{
		auto request_body = request->GetRequestBody();
		auto json = ov::Json::Parse(request_body);
	});

	http_interceptor->Register(HttpMethod::Post, "/api/logout", [this](const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) -> void
	{
		auto request_body = request->GetRequestBody();
		auto json = ov::Json::Parse(request_body);

		logti("BODY: %s", json.ToString().CStr());
	});

	http_interceptor->Register(HttpMethod::Get, ".*", [document_root, this](const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response) -> void
	{
		auto path = ov::PathManager::Combine(document_root, request->GetUri());
		auto real_path = ov::PathManager::GetCanonicalPath(path);

		if(real_path.IsEmpty())
		{
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();
			return;
		}

		if(real_path.HasPrefix(document_root) == false)
		{
			// If the file exists but is not in the DocumentRoot, it is not provided because of the risk of attack
			logtw("The client requested a resource, but the resource not in DocumentRoot: %s", real_path.CStr());
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();
			return;
		}

		FILE *file = ::fopen(real_path, "rb");

		if(file == nullptr)
		{
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();
			return;
		}

		auto data = std::make_shared<ov::Data>();
		ov::ByteStream stream(data.get());
		char buffer[4096];

		while(!feof(file))
		{
			auto read_bytes = ::fread(buffer, 1, OV_COUNTOF(buffer), file);

			if(read_bytes == 0)
			{
				break;
			}

			stream.Append(buffer, read_bytes);
		}

		if(response->GetStatusCode() == HttpStatusCode::OK)
		{
			response->SetHeader("Content-Length", ov::String::FormatString("%zu", data->GetLength()));
			response->SetHeader("Content-Type", "text/html");
			response->AppendData(data);
		}

		::fclose(file);

		response->Response();
	});

	return _http_server->AddInterceptor(http_interceptor);
}

