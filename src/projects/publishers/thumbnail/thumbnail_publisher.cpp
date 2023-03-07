#include "thumbnail_publisher.h"

#include <base/ovlibrary/url.h>

#include "thumbnail_private.h"

#define UNUSED(expr)  \
	do                \
	{                 \
		(void)(expr); \
	} while (0)

std::shared_ptr<ThumbnailPublisher> ThumbnailPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto file = std::make_shared<ThumbnailPublisher>(server_config, router);

	if (!file->Start())
	{
		return nullptr;
	}

	return file;
}

ThumbnailPublisher::ThumbnailPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
	logtd("ThumbnailPublisher has been create");
}

ThumbnailPublisher::~ThumbnailPublisher()
{
	logtd("ThumbnailPublisher has been terminated finally");
}

bool ThumbnailPublisher::PrepareHttpServers(
	const std::vector<ov::String> &ip_list,
	const bool is_port_configured, const uint16_t port,
	const bool is_tls_port_configured, const uint16_t tls_port,
	const int worker_count)
{
	auto http_server_manager = http::svr::HttpServerManager::GetInstance();

	std::vector<std::shared_ptr<http::svr::HttpServer>> http_server_list;
	std::vector<std::shared_ptr<http::svr::HttpsServer>> https_server_list;

	if (http_server_manager->CreateServers(
			GetPublisherName(), "Thumb",
			&http_server_list, &https_server_list,
			ip_list,
			is_port_configured, port,
			is_tls_port_configured, tls_port,
			nullptr,
			false,
			[&](const ov::SocketAddress &address, bool is_https, const std::shared_ptr<http::svr::HttpServer> &http_server) {
				http_server->AddInterceptor(CreateInterceptor());
			},
			worker_count))
	{
		_http_server_list = std::move(http_server_list);
		_https_server_list = std::move(https_server_list);

		return true;
	}

	http_server_manager->ReleaseServers(&http_server_list);
	http_server_manager->ReleaseServers(&https_server_list);

	return false;
}

bool ThumbnailPublisher::Start()
{
	auto server_config = GetServerConfig();

	const auto &thumbnail_bind_config = server_config.GetBind().GetPublishers().GetThumbnail();
	if (thumbnail_bind_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_configured = false;
	auto worker_count = thumbnail_bind_config.GetWorkerCount(&is_configured);
	worker_count = is_configured ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	bool is_port_configured;
	auto &port_config = thumbnail_bind_config.GetPort(&is_port_configured);

	bool is_tls_port_configured;
	auto &tls_port_config = thumbnail_bind_config.GetTlsPort(&is_tls_port_configured);

	if ((is_port_configured == false) && (is_tls_port_configured == false))
	{
		logtw("%s is disabled - No port is configured", GetPublisherName());
		return true;
	}

	return PrepareHttpServers(
			   server_config.GetIPList(),
			   is_port_configured, port_config.GetPort(),
			   is_tls_port_configured, tls_port_config.GetPort(),
			   worker_count) &&
		   Publisher::Start();
}

bool ThumbnailPublisher::Stop()
{
	auto manager = http::svr::HttpServerManager::GetInstance();

	_http_server_list_mutex.lock();
	auto http_server_list = std::move(_http_server_list);
	auto https_server_list = std::move(_https_server_list);
	_http_server_list_mutex.unlock();

	manager->ReleaseServers(&http_server_list);
	manager->ReleaseServers(&https_server_list);

	return Publisher::Stop();
}

bool ThumbnailPublisher::OnCreateHost(const info::Host &host_info)
{
	bool result = true;
	auto certificate = host_info.GetCertificate();

	if (certificate != nullptr)
	{
		std::lock_guard lock_guard{_http_server_list_mutex};

		for (auto &https_server : _https_server_list)
		{
			if (https_server->AppendCertificate(certificate) != nullptr)
			{
				result = false;
			}
		}
	}

	return result;
}

bool ThumbnailPublisher::OnDeleteHost(const info::Host &host_info)
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

std::shared_ptr<pub::Application> ThumbnailPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return ThumbnailApplication::Create(ThumbnailPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool ThumbnailPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto file_application = std::static_pointer_cast<ThumbnailApplication>(application);
	if (file_application == nullptr)
	{
		logte("Could not found thumbnail application. app:%s", file_application->GetName().CStr());
		return false;
	}

	return true;
}

std::shared_ptr<ThumbnailInterceptor> ThumbnailPublisher::CreateInterceptor()
{
	auto http_interceptor = std::make_shared<ThumbnailInterceptor>();

	http_interceptor->Register(http::Method::Get, R"(.+thumb\.(jpg|png)$)", [this](const std::shared_ptr<http::svr::HttpExchange> &exchange) -> http::svr::NextHandler {
		auto request = exchange->GetRequest();

		auto request_url = request->GetParsedUri();
		if (request_url == nullptr)
		{
			logtw("Failed to parse url: %s", request->GetRequestTarget().CStr());
			return http::svr::NextHandler::Call;
		}

		if (request_url->App().IsEmpty() == true || request_url->Stream().IsEmpty() == true || request_url->File().IsEmpty() == true)
		{
			logtw("Invalid request url: %s", request->GetUri().CStr());
			return http::svr::NextHandler::Call;
		}

		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(request_url->Host(), request_url->App());
		if (vhost_app_name.IsValid() == false)
		{
			logtw("Could not found virtual host");
			return http::svr::NextHandler::Call;
		}

		auto app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(vhost_app_name));
		if (app_info == nullptr)
		{
			logtw("Could not found application");
			return http::svr::NextHandler::Call;
		}

		auto app_config = app_info->GetConfig();
		auto thumbnail_config = app_config.GetPublishers().GetThumbnailPublisher();
		auto response = exchange->GetResponse();

		// Check CORS
		auto application = std::static_pointer_cast<ThumbnailApplication>(GetApplicationByName(vhost_app_name));
		if (application == nullptr)
		{
			response->AppendString("Could not found application of thumbnail publisher");
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();

			exchange->Release();

			return http::svr::NextHandler::DoNotCall;
		}

		application->GetCorsManager().SetupHttpCorsHeader(vhost_app_name, request, response);

		auto host_name = request_url->Host();
		auto stream_name = request_url->Stream();

		// Check Stream
		auto stream = GetStream(vhost_app_name, request_url->Stream());		
		if (stream == nullptr)
		{
			// If the stream does not exists, request to the provider
			stream = PullStream(request_url, vhost_app_name, host_name, stream_name);
			if (stream == nullptr)
			{
				logte("There is no stream or cannot pull a stream : %s", request_url->Stream().CStr());
				response->AppendString("There is no stream or cannot pull a stream");
				response->SetStatusCode(http::StatusCode::NotFound);
				response->Response();
				exchange->Release();				
				return http::svr::NextHandler::DoNotCall;
			}			
		}

		if (stream->WaitUntilStart(100000) == false)
		{
			logtw("(%s/%s) stream has not started", vhost_app_name.CStr(), stream_name.CStr());
			response->AppendString(ov::String::FormatString("stream has not started"));
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();
			exchange->Release();							
			return http::svr::NextHandler::DoNotCall;
		}

		// Check Extentions
		auto media_codec_id = cmn::MediaCodecId::None;
		if (request_url->File().LowerCaseString().IndexOf(".jpg") >= 0)
		{
			media_codec_id = cmn::MediaCodecId::Jpeg;
		}
		else if (request_url->File().LowerCaseString().IndexOf(".png") >= 0)
		{
			media_codec_id = cmn::MediaCodecId::Png;
		}

		// Wait O seconds for thumbnail image to be received
		auto endcoded_video_frame = std::static_pointer_cast<ThumbnailStream>(stream)->GetVideoFrameByCodecId(media_codec_id, 5000);
		if (endcoded_video_frame == nullptr)
		{
			response->AppendString(ov::String::FormatString("There is no thumbnail image"));
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();
			exchange->Release();							

			return http::svr::NextHandler::DoNotCall;
		}

		response->SetHeader("Content-Type", (media_codec_id == cmn::MediaCodecId::Jpeg) ? "image/jpeg" : "image/png");
		response->SetStatusCode(http::StatusCode::OK);
		response->AppendData(std::move(endcoded_video_frame->Clone()));
		auto sent_size = response->Response();
		exchange->Release();

		MonitorInstance->IncreaseBytesOut(*stream, PublisherType::Thumbnail, sent_size);

		return http::svr::NextHandler::DoNotCall;
	});

	return http_interceptor;
} 


// @Refer to segment_stream_server.cpp
bool ThumbnailPublisher::SetAllowOrigin(const ov::String &origin_url, std::vector<ov::String> &cors_urls, const std::shared_ptr<http::svr::HttpResponse> &response)
{
	if (cors_urls.empty())
	{
		// Not need to check CORS
		response->SetHeader("Access-Control-Allow-Origin", "*");
		return true;
	}

	auto item = std::find_if(cors_urls.begin(), cors_urls.end(),
							 [&origin_url](auto &url) -> bool {
								 if (url.HasPrefix("http://*."))
									 return origin_url.HasSuffix(url.Substring(strlen("http://*")));
								 else if (url.HasPrefix("https://*."))
									 return origin_url.HasSuffix(url.Substring(strlen("https://*")));

								 return (origin_url == url);
							 });

	if (item == cors_urls.end())
	{
		return false;
	}

	response->SetHeader("Access-Control-Allow-Origin", origin_url);

	return true;
}
