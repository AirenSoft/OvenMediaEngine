//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_stream_server.h"

#include <modules/http/server/http_server_manager.h>
#include <monitoring/monitoring.h>

#include <regex>
#include <sstream>

#include "segment_stream_private.h"

SegmentStreamServer::SegmentStreamServer()
{
}

std::shared_ptr<SegmentStreamInterceptor> SegmentStreamServer::CreateInterceptor()
{
	return std::make_shared<SegmentStreamInterceptor>();
}

bool SegmentStreamServer::Start(const cfg::Server &server_config,
								const ov::SocketAddress *address,
								const ov::SocketAddress *tls_address,
								bool disable_http2_force,
								int thread_count,
								int worker_count)
{
	if ((_http_server != nullptr) || (_https_server != nullptr))
	{
		OV_ASSERT(false, "Server is already running (%p, %p)", _http_server.get(), _https_server.get());
		return false;
	}

	auto process_handler = std::bind(&SegmentStreamServer::ProcessRequest, this, std::placeholders::_1);

	bool result = true;
	auto manager = http::svr::HttpServerManager::GetInstance();

	std::shared_ptr<http::svr::HttpServer> http_server = (address != nullptr) ? manager->CreateHttpServer("SegPub", *address, worker_count) : nullptr;
	result = result && ((address != nullptr) ? (http_server != nullptr) : true);

	std::shared_ptr<http::svr::HttpsServer> https_server = nullptr;
	if (result && (tls_address != nullptr))
	{
		https_server = manager->CreateHttpsServer("SegPub", *tls_address, disable_http2_force, worker_count);
	}

	result = result && ((tls_address != nullptr) ? (https_server != nullptr) : true);

	result = result && PrepareInterceptors(http_server, https_server, thread_count, process_handler);

	if (result)
	{
		_http_server = http_server;
		_https_server = https_server;
	}
	else
	{
		// Rollback
		manager->ReleaseServer(http_server);
		manager->ReleaseServer(https_server);
	}

	return result;
}

bool SegmentStreamServer::Stop()
{
	// Remove Interceptors

	// Stop server
	if (_http_server != nullptr)
	{
		_http_server->Stop();
		_http_server = nullptr;
	}

	if (_https_server != nullptr)
	{
		_https_server->Stop();
		_https_server = nullptr;
	}

	return false;
}

bool SegmentStreamServer::AppendCertificate(const std::shared_ptr<const info::Certificate> &certificate)
{
	if(_https_server != nullptr && certificate != nullptr)
	{
		return _https_server->AppendCertificate(certificate) == nullptr;
	}

	return true;
}

bool SegmentStreamServer::RemoveCertificate(const std::shared_ptr<const info::Certificate> &certificate)
{
	if(_https_server != nullptr && certificate != nullptr)
	{
		return _https_server->RemoveCertificate(certificate) == nullptr;
	}

	return true;
}


bool SegmentStreamServer::AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer)
{
	for (const auto &item : _observers)
	{
		if (item == observer)
		{
			logtw("%p is already observer of SegmentStreamServer", observer.get());
			return false;
		}
	}

	_observers.push_back(observer);

	return true;
}

bool SegmentStreamServer::RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer)
{
	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&](std::shared_ptr<SegmentStreamObserver> const &value) -> bool {
								 return value == observer;
							 });

	if (item == _observers.end())
	{
		logtw("%p is not registered observer", observer.get());
		return false;
	}

	_observers.erase(item);

	return true;
}

bool SegmentStreamServer::Disconnect(const ov::String &app_name, const ov::String &stream_name)
{
	return true;
}

bool SegmentStreamServer::PrepareInterceptors(
	const std::shared_ptr<http::svr::HttpServer> &http_server,
	const std::shared_ptr<http::svr::HttpsServer> &https_server,
	int thread_count, const SegmentProcessHandler &process_handler)
{
	auto segment_stream_interceptor = CreateInterceptor();

	if (segment_stream_interceptor == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	bool result = true;

	result = result && ((http_server == nullptr) || http_server->AddInterceptor(segment_stream_interceptor));
	result = result && ((https_server == nullptr) || https_server->AddInterceptor(segment_stream_interceptor));

	result = result && segment_stream_interceptor->Start(thread_count, process_handler);

	return result;
}

bool SegmentStreamServer::ProcessRequest(const std::shared_ptr<http::svr::HttpExchange> &client)
{
	auto response = client->GetResponse();
	auto request = client->GetRequest();

	// Set default headers
	response->SetHeader("Server", "OvenMediaEngine");
	response->SetHeader("Content-Type", "text/html");

	// Parse URL (URL must be "app/stream/file.ext" format)
	auto request_uri = request->GetUri();
	auto url = ov::Url::Parse(request_uri);
	if (url == nullptr)
	{
		logtw("Failed to parse URL: %s", request_uri.CStr());
		response->SetStatusCode(http::StatusCode::BadRequest);
		return false;
	}

	if (url->Path() == "crossdomain.xml")
	{
		_cors_manager.SetupRtmpCorsXml(response);
		return false;
	}

	auto host_header = request->GetHost();
	auto host_name = host_header.Split(":")[0];
	auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, url->App());

	if (vhost_app_name.IsValid() == false)
	{
		logtw("Invalid app name for host: %s, app: %s", host_name.CStr(), url->App().CStr());
		response->SetStatusCode(http::StatusCode::BadRequest);
		return false;
	}

	_cors_manager.SetupHttpCorsHeader(vhost_app_name, request, response);

	SegmentStreamRequestInfo request_info(
		vhost_app_name,
		host_name, url->Stream(), url->File());

	auto tokens = url->File().Split(".");
	auto file_ext = (tokens.size() >= 2) ? tokens[1] : "";

	return ProcessStreamRequest(client, request_info, file_ext);
}

void SegmentStreamServer::SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list)
{
	_cors_manager.SetCrossDomains(vhost_app_name, url_list);
}

std::shared_ptr<pub::Stream> SegmentStreamServer::GetStream(const std::shared_ptr<http::svr::HttpExchange> &client)
{
	return client->GetExtraAs<pub::Stream>();
}

std::shared_ptr<mon::StreamMetrics> SegmentStreamServer::GetStreamMetric(const std::shared_ptr<http::svr::HttpExchange> &client)
{
	auto stream_info = GetStream(client);
	if (stream_info == nullptr)
	{
		return nullptr;
	}

	auto stream_metric = StreamMetrics(*stream_info);
	if (stream_metric == nullptr)
	{
		return nullptr;
	}

	return stream_metric;
}