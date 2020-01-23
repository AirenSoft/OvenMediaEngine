//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_stream_server.h"
#include <regex>
#include <sstream>
#include "segment_stream_private.h"

SegmentStreamServer::SegmentStreamServer()
{
	_cross_domain_xml =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
		"<!DOCTYPE cross-domain-policy SYSTEM \"http://www.adobe.com/xml/dtds/cross-domain-policy.dtd\">\n"
		"<cross-domain-policy>\n"
		"\t<allow-access-from domain=\"*\" secure=\"false\"/>\n"
		"\t<site-control permitted-cross-domain-policies=\"all\"/>\n"
		"</cross-domain-policy>";
}

bool SegmentStreamServer::Start(const ov::SocketAddress &address,
								std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
								const ov::String &app_name,
								int thread_count,
								const std::shared_ptr<Certificate> &certificate,
								const std::shared_ptr<Certificate> &chain_certificate)
{
	if (_http_server != nullptr)
	{
		OV_ASSERT(false, "Server is already running");
		return false;
	}
	_app_name = app_name;

	// Interceptor add
	// - handler register
	auto process_handler = std::bind(&SegmentStreamServer::ProcessRequest,
									 this,
									 std::placeholders::_1,
									 std::placeholders::_2,
									 std::placeholders::_3);

	auto segment_stream_interceptor = CreateInterceptor();
	segment_stream_interceptor->Start(thread_count, process_handler);

	//    segment_stream_interceptor->Register(HttpMethod::Get, "", process_func, false);

	// same port http server check
	if (http_server_manager.find(address.Port()) != http_server_manager.end())
	{
		auto item = http_server_manager.find(address.Port());
		auto http_server = item->second;

		segment_stream_interceptor->SetCrossdomainBlock();
		http_server->AddInterceptor(segment_stream_interceptor);
		_http_server = http_server;

		return true;
	}

	if (certificate != nullptr)
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

	http_server_manager[address.Port()] = _http_server;

	_http_server->AddInterceptor(segment_stream_interceptor);

	return _http_server->Start(address);
}

bool SegmentStreamServer::Stop()
{
	// Remove Interceptor

	return false;
}

bool SegmentStreamServer::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &stream_collections)
{
	return true;
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

//====================================================================================================
// ParseRequestUrl
// - URL 분리
//  ex) ..../app_name/stream_name/file_name.file_ext?param=param_value
//====================================================================================================
bool SegmentStreamServer::ParseRequestUrl(const ov::String &request_url,
										  ov::String &app_name,
										  ov::String &stream_name,
										  ov::String &file_name,
										  ov::String &file_ext)
{
	ov::String request_path;
	ov::String request_param;

	// 확장자 확인
	// 파라메터 분리  directory/file.ext?param=test
	auto tokens = request_url.Split("?");
	if (tokens.size() == 0)
	{
		return false;
	}

	request_path = tokens[0];
	request_param = tokens.size() == 2 ? tokens[1] : "";

	// ...../app_name/stream_name/file_name.ext_name 분리
	tokens.clear();
	tokens = request_path.Split("/");

	if (tokens.size() < 3)
	{
		return false;
	}

	app_name = tokens[tokens.size() - 3];
	stream_name = tokens[tokens.size() - 2];
	file_name = tokens[tokens.size() - 1];

	// file_name.ext_name 분리
	tokens.clear();
	tokens = file_name.Split(".");

	if (tokens.size() != 2)
	{
		return false;
	}

	file_ext = tokens[1];

	/*
    logtd("request : %s\n"\
         "request path : %s\n"\
         "request param : %s\n"\
         "app name : %s\n"\
         "stream name : %s\n"\
         "file name : %s\n"\
         "file ext : %s\n",
          request_url.CStr(), request_path.CStr(), request_param.CStr(),  app_name.CStr(), stream_name.CStr(), file_name.CStr(),file_ext.CStr());
    */

	return true;
}

bool SegmentStreamServer::ProcessRequest(const std::shared_ptr<HttpClient> &client,
										 const ov::String &request_target,
										 const ov::String &origin_url)
{
	auto &response = client->GetResponse();
	HttpConnection connetion = HttpConnection::Closed;

	do
	{
		ov::String app_name;
		ov::String stream_name;
		ov::String file_name;
		ov::String file_ext;

		// Set default headers
		response->SetHeader("Server", "OvenMediaEngine");
		response->SetHeader("Content-Type", "text/html");

		// Check crossdomains
		if (request_target.IndexOf("crossdomain.xml") >= 0)
		{
			response->SetHeader("Content-Type", "text/x-cross-domain-policy");
			response->AppendString(_cross_domain_xml);
			break;
		}

		// Parse URL (URL must be "app/stream/file.ext" format)
		if (ParseRequestUrl(request_target, app_name, stream_name, file_name, file_ext) == false)
		{
			logtd("Failed to parse URL: %s", request_target.CStr());
			response->SetStatusCode(HttpStatusCode::NotFound);
			break;
		}

		// Check CORS
		if (origin_url.IsEmpty() == false)
		{
			SetAllowOrigin(origin_url, response);
		}

		// Check app name
		if (_app_name != app_name)
		{
			logtd("Failed to check app name: %s", request_target.CStr());
			response->SetStatusCode(HttpStatusCode::NotFound);
			break;
		}
		auto tokens = client->GetRequest()->GetUri().Split("/");
		auto host_name = client->GetRequest()->GetHeader("HOST").Split(":")[0];
		ov::String internal_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, tokens[1]);

		connetion = ProcessRequestStream(client, internal_app_name, stream_name, file_name, file_ext);
	} while (false);

	switch (connetion)
	{
		case HttpConnection::Closed:
			return response->Close();

		case HttpConnection::KeepAlive:
			return true;

		default:
			response->Close();
			OV_ASSERT2(false);
			return false;
	}
}

bool SegmentStreamServer::SetAllowOrigin(const ov::String &origin_url, const std::shared_ptr<HttpResponse> &response)
{
	if (_cors_urls.empty())
	{
		// Not need to check CORS
		response->SetHeader("Access-Control-Allow-Origin", "*");
		return true;
	}

	auto item = std::find_if(_cors_urls.begin(), _cors_urls.end(),
							 [&origin_url](auto &url) -> bool {
								 if (url.HasPrefix("http://*."))
									 return origin_url.HasSuffix(url.Substring(strlen("http://*")));
								 else if (url.HasPrefix("https://*."))
									 return origin_url.HasSuffix(url.Substring(strlen("https://*")));

								 return (origin_url == url);
							 });

	if (item == _cors_urls.end())
	{
		return false;
	}

	// response->SetHeader("Access-Control-Allow-Credentials", "true");
	// response->SetHeader("Access-Control-Allow-Headers", "Content-Type, *");
	response->SetHeader("Access-Control-Allow-Origin", origin_url);

	return true;
}

//====================================================================================================
// SetCrossDomain Parsing/Setting
//  crossdoamin : only domain
//  CORS : http/htts check
//
// <Url>*</Url>
// <Url>*.ovenplayer.com</Url>
// <Url>http://demo.ovenplayer.com</Url>
// <Url>https://demo.ovenplayer.com</Url>
// <Url>http://*.ovenplayer.com</Url>
//====================================================================================================
void SegmentStreamServer::SetCrossDomain(const std::vector<cfg::Url> &url_list)
{
	std::vector<ov::String> crossdmain_urls;
	ov::String http_prefix = "http://";
	ov::String https_prefix = "https://";

	if (url_list.empty())
	{
		return;
	}

	for (auto &url_item : url_list)
	{
		ov::String url = url_item.GetUrl();

		// all access allow
		if (url == "*")
		{
			crossdmain_urls.clear();
			_cors_urls.clear();
			return;
		}

		// http
		if (url.HasPrefix(http_prefix))
		{
			if (!UrlExistCheck(crossdmain_urls, url.Substring(http_prefix.GetLength())))
				crossdmain_urls.push_back(url.Substring(http_prefix.GetLength()));

			if (!UrlExistCheck(_cors_urls, url))
				_cors_urls.push_back(url);
		}
		// https
		else if (url.HasPrefix(https_prefix))
		{
			if (!UrlExistCheck(crossdmain_urls, url.Substring(https_prefix.GetLength())))
				crossdmain_urls.push_back(url.Substring(https_prefix.GetLength()));

			if (!UrlExistCheck(_cors_urls, url))
				_cors_urls.push_back(url);
		}
		// only domain
		else
		{
			if (!UrlExistCheck(crossdmain_urls, url))
				crossdmain_urls.push_back(url);

			if (!UrlExistCheck(_cors_urls, http_prefix + url))
				_cors_urls.push_back(http_prefix + url);

			if (!UrlExistCheck(_cors_urls, https_prefix + url))
				_cors_urls.push_back(https_prefix + url);
		}
	}

	// crossdomain.xml
	std::ostringstream cross_domain_xml;

	cross_domain_xml << "<?xml version=\"1.0\"?>\r\n";
	cross_domain_xml << "<cross-domain-policy>\r\n";
	for (auto &url : crossdmain_urls)
	{
		cross_domain_xml << "    <allow-access-from domain=\"" << url.CStr() << "\"/>\r\n";
	}
	cross_domain_xml << "</cross-domain-policy>";

	_cross_domain_xml = cross_domain_xml.str().c_str();
	ov::String cors_urls;
	for (auto &url : _cors_urls)
		cors_urls += url + "\n";
	logtd("<%s> CORS \n%s", _app_name.CStr(), cors_urls.CStr());

	logtd("<%s> crossdomain.xml \n%s", _app_name.CStr(), _cross_domain_xml.CStr());
}

bool SegmentStreamServer::UrlExistCheck(const std::vector<ov::String> &url_list, const ov::String &check_url)
{
	auto item = std::find_if(url_list.begin(), url_list.end(),
							 [&check_url](auto &url) -> bool {
								 return check_url == url;
							 });

	return (item != url_list.end());
}
