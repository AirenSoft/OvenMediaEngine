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

bool SegmentStreamServer::Start(const ov::SocketAddress *address,
								const ov::SocketAddress *tls_address,
								int thread_count,
								int worker_count)
{
	if ((_http_server != nullptr) || (_https_server != nullptr))
	{
		OV_ASSERT(false, "Server is already running (%p, %p)", _http_server.get(), _https_server.get());
		return false;
	}

	auto process_handler = std::bind(&SegmentStreamServer::ProcessRequest, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	bool result = true;
	auto vhost_list = ocst::Orchestrator::GetInstance()->GetVirtualHostList();

	auto manager = http::svr::HttpServerManager::GetInstance();

	std::shared_ptr<http::svr::HttpServer> http_server = (address != nullptr) ? manager->CreateHttpServer("SegPub", *address, worker_count) : nullptr;
	result = result && ((address != nullptr) ? (http_server != nullptr) : true);

	std::shared_ptr<http::svr::HttpsServer> https_server = (tls_address != nullptr) ? manager->CreateHttpsServer("SegPub", *tls_address, vhost_list, worker_count) : nullptr;
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

bool SegmentStreamServer::AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer)
{
	// 기존에 등록된 observer가 있는지 확인
	for (const auto &item : _observers)
	{
		if (item == observer)
		{
			// 기존에 등록되어 있음
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
		// 기존에 등록되어 있지 않음
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

bool SegmentStreamServer::ProcessRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
										 const ov::String &request_target,
										 const ov::String &origin_url)
{
	auto response = client->GetResponse();
	auto request = client->GetRequest();
	http::svr::ConnectionPolicy connection_policy = http::svr::ConnectionPolicy::Closed;

	do
	{
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
			break;
		}

		if (url->Path() == "crossdomain.xml")
		{
			SetRtmpCorsHeaders(response);
			break;
		}

		auto host_header = request->GetHeader("HOST");
		auto host_name = host_header.Split(":")[0];
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, url->App());

		if (vhost_app_name.IsValid() == false)
		{
			logtw("Invalid app name for host: %s, app: %s", host_name.CStr(), url->App().CStr());
			response->SetStatusCode(http::StatusCode::BadRequest);
			break;
		}
		else
		{
			if (origin_url.IsEmpty())
			{
				// Set CORS headers using HOST header
				SetHttpCorsHeaders(vhost_app_name, host_header, url, response);
			}
			else
			{
				SetHttpCorsHeaders(vhost_app_name, origin_url, url, response);
			}
		}

		SegmentStreamRequestInfo request_info(
			vhost_app_name,
			host_name, url->Stream(), url->File());

		auto tokens = url->File().Split(".");
		auto file_ext = (tokens.size() >= 2) ? tokens[1] : "";

		connection_policy = ProcessStreamRequest(client, request_info, file_ext);
	} while (false);

	switch (connection_policy)
	{
		case http::svr::ConnectionPolicy::Closed:
			return response->Close();

		case http::svr::ConnectionPolicy::KeepAlive:
			return true;

		default:
			response->Close();
			OV_ASSERT2(false);
			return false;
	}
}

bool SegmentStreamServer::SetRtmpCorsHeaders(const std::shared_ptr<http::svr::HttpResponse> &response)
{
	std::lock_guard lock_guard(_cors_mutex);

	if (_cors_rtmp.IsEmpty() == false)
	{
		response->SetHeader("Content-Type", "text/x-cross-domain-policy");
		response->AppendString(_cors_rtmp);
	}

	return true;
}

bool SegmentStreamServer::SetHttpCorsHeaders(const info::VHostAppName &vhost_app_name, const ov::String &origin, const std::shared_ptr<const ov::Url> &url, const std::shared_ptr<http::svr::HttpResponse> &response)
{
	ov::String cors_header = "";

	{
		std::lock_guard lock_guard(_cors_mutex);

		auto cors_policy_iterator = _cors_policy_map.find(vhost_app_name);
		auto cors_domains_iterator = _cors_http_map.find(vhost_app_name);

		if (
			(cors_policy_iterator == _cors_policy_map.end()) ||
			(cors_domains_iterator == _cors_http_map.end()))
		{
			// This happens in the following situations:
			//
			// 1) Request to an application that doesn't exist
			// 2) Request while the application is being created
			return false;
		}

		const auto &cors_policy = cors_policy_iterator->second;

		switch (cors_policy)
		{
			case CorsPolicy::Empty:
				// Nothing to do
				return true;

			case CorsPolicy::All:
				cors_header = "*";
				break;

			case CorsPolicy::Null:
				cors_header = "null";
				break;

			case CorsPolicy::Origin: {
				const auto &cors_domains = cors_domains_iterator->second;
				auto item = std::find_if(cors_domains.begin(), cors_domains.end(),
										 [&origin](auto &cors_domain) -> bool {
											 return (origin == cors_domain);
										 });

				if (item == cors_domains.end())
				{
					// Could not find the domain
					return false;
				}

				cors_header = origin;
			}
		}
	}

	response->SetHeader("Access-Control-Allow-Origin", cors_header);
	response->SetHeader("Vary", "Origin");

	response->SetHeader("Access-Control-Allow-Credentials", "true");
	response->SetHeader("Access-Control-Allow-Methods", "GET");
	response->SetHeader("Access-Control-Allow-Headers", "*");

	return true;
}

void SegmentStreamServer::SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list)
{
	{
		std::lock_guard lock_guard(_cors_mutex);

		auto &cors_urls = _cors_http_map[vhost_app_name];
		auto &cors_policy = _cors_policy_map[vhost_app_name];
		ov::String cors_rtmp;

		auto cors_domains_for_rtmp = std::vector<ov::String>();

		cors_urls.clear();
		cors_rtmp = "";

		if (url_list.size() == 0)
		{
			cors_policy = CorsPolicy::Empty;
			return;
		}

		cors_policy = CorsPolicy::Origin;

		constexpr char HTTP_PREFIX[] = "http://";
		constexpr auto HTTP_PREFIX_LENGTH = OV_COUNTOF(HTTP_PREFIX) - 1;
		constexpr char HTTPS_PREFIX[] = "https://";
		constexpr auto HTTPS_PREFIX_LENGTH = OV_COUNTOF(HTTPS_PREFIX) - 1;

		for (auto url : url_list)
		{
			if (url == "*")
			{
				cors_urls.clear();
				cors_urls.push_back("*");
				cors_policy = CorsPolicy::All;

				if (url_list.size() > 1)
				{
					// Ignore other items if "*" is specified
					logtw("Invalid CORS settings found for %s: '*' cannot be used like other items. Other items are ignored.", vhost_app_name.CStr());
				}

				break;
			}
			else if (url == "null")
			{
				if (url_list.size() > 1)
				{
					logtw("Invalid CORS settings found for %s: '*' cannot be used like other items. 'null' item is ignored.", vhost_app_name.CStr());
				}
				else
				{
					cors_policy = CorsPolicy::Null;
				}

				continue;
			}

			cors_urls.push_back(url);

			if (url.HasPrefix(HTTP_PREFIX))
			{
				url = url.Substring(HTTP_PREFIX_LENGTH);
			}
			else if (url.HasPrefix(HTTPS_PREFIX))
			{
				url = url.Substring(HTTPS_PREFIX_LENGTH);
			}

			cors_domains_for_rtmp.push_back(url);
		}

		// Create crossdomain.xml for RTMP - OME does not support RTMP playback, so it is not used.
		std::ostringstream cross_domain_xml;

		cross_domain_xml
			<< R"(<?xml version="1.0"?>)" << std::endl
			<< R"(<!DOCTYPE cross-domain-policy SYSTEM "http://www.adobe.com/xml/dtds/cross-domain-policy.dtd">)" << std::endl
			<< R"(<cross-domain-policy>)" << std::endl;

		if (cors_policy == CorsPolicy::All)
		{
			cross_domain_xml
				<< R"(	<site-control permitted-cross-domain-policies="all" />)" << std::endl
				<< R"(	<allow-access-from domain="*" secure="false" />)" << std::endl
				<< R"(	<allow-http-request-headers-from domain="*" headers="*" secure="false"/>)" << std::endl;
		}
		else
		{
			for (auto &cors_domain : cors_urls)
			{
				cross_domain_xml
					<< R"(	<allow-access-from domain=")" << cors_domain.CStr() << R"(" />)" << std::endl;
			}
		}

		cross_domain_xml
			<< R"(</cross-domain-policy>)";

		cors_rtmp = cross_domain_xml.str().c_str();

		if (_cors_rtmp.IsEmpty())
		{
			_cors_rtmp = cors_rtmp;
		}
		else
		{
			if (cors_rtmp != _cors_rtmp)
			{
				logtw("Different CORS settings found for RTMP: crossdomain.xml must be located / and cannot be declared per app");
				logtw("This CORS settings will be used\n%s", _cors_rtmp.CStr());
			}
		}
	}
}

bool SegmentStreamServer::UrlExistCheck(const std::vector<ov::String> &url_list, const ov::String &check_url)
{
	auto item = std::find_if(url_list.begin(), url_list.end(),
							 [&check_url](auto &url) -> bool {
								 return check_url == url;
							 });

	return (item != url_list.end());
}

std::shared_ptr<pub::Stream> SegmentStreamServer::GetStream(const std::shared_ptr<http::svr::HttpConnection> &client)
{
	auto request = client->GetRequest();
	return request->GetExtraAs<pub::Stream>();
}

std::shared_ptr<mon::StreamMetrics> SegmentStreamServer::GetStreamMetric(const std::shared_ptr<http::svr::HttpConnection> &client)
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