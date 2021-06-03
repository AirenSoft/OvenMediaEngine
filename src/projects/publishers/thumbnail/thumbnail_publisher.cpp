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

bool ThumbnailPublisher::Start()
{
	auto manager = http::svr::HttpServerManager::GetInstance();

	auto server_config = GetServerConfig();

	const auto &thumbnail_bind_config = server_config.GetBind().GetPublishers().GetThumbnail();
	if (thumbnail_bind_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	bool is_parsed = false;

	auto worker_count = thumbnail_bind_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	// Initialze HTTP Server
	bool http_server_result = true;
	auto &port = thumbnail_bind_config.GetPort(&is_parsed);
	if (is_parsed)
	{
		ov::SocketAddress address = ov::SocketAddress(server_config.GetIp(), port.GetPort());

		_http_server = manager->CreateHttpServer("thumb_http", address, worker_count);

		if (_http_server != nullptr)
		{
			_http_server->AddInterceptor(CreateInterceptor());
		}
		else
		{
			logte("Could not initialize thumbnail http server");
			http_server_result = false;
		}
	}

	// Initialze HTTPS Server
	bool https_server_result = true;
	auto &tls_port = thumbnail_bind_config.GetTlsPort(&is_parsed);
	if (is_parsed)
	{
		ov::SocketAddress tls_address = ov::SocketAddress(server_config.GetIp(), tls_port.GetPort());

		auto vhost_list = ocst::Orchestrator::GetInstance()->GetVirtualHostList();

		_https_server = manager->CreateHttpsServer("thumb_https", tls_address, vhost_list, worker_count);
		if (_https_server != nullptr)
		{
			_https_server->AddInterceptor(CreateInterceptor());
		}
		else
		{
			logte("Could not initialize thumbnail https server");
			https_server_result = false;
		}
	}

	if (http_server_result == false && _http_server != nullptr)
	{
		manager->ReleaseServer(_http_server);
	}

	if (https_server_result == false && _https_server != nullptr)
	{
		manager->ReleaseServer(_https_server);
	}

	if (http_server_result || https_server_result)
	{
		logti("Thumbnail publisher is listening on %s", server_config.GetIp().CStr());
	}

	return Publisher::Start();
}

bool ThumbnailPublisher::Stop()
{
	auto manager = http::svr::HttpServerManager::GetInstance();

	std::shared_ptr<http::svr::HttpServer> http_server = std::move(_http_server);
	std::shared_ptr<http::svr::HttpsServer> https_server = std::move(_https_server);

	if (http_server != nullptr)
	{
		manager->ReleaseServer(_http_server);
	}

	if (https_server != nullptr)
	{
		manager->ReleaseServer(https_server);
	}

	return Publisher::Stop();
}

std::shared_ptr<http::svr::RequestInterceptor> ThumbnailPublisher::CreateInterceptor()
{
	auto http_interceptor = std::make_shared<http::svr::DefaultInterceptor>();

	http_interceptor->Register(http::Method::Get, R"(.+thumb\.(jpg|png)$)", [this](const std::shared_ptr<http::svr::HttpConnection> &client) -> http::svr::NextHandler {
		auto request = client->GetRequest();

		ov::String request_param;
		ov::String app_name;
		ov::String stream_name;
		ov::String file_name;
		ov::String file_ext;

		// Parse Url
		if (ParseRequestUrl(request->GetRequestTarget(), request_param, app_name, stream_name, file_name, file_ext) == false)
		{
			logte("Failed to parse URL: %s", request->GetRequestTarget().CStr());
			return http::svr::NextHandler::Call;
		}

		auto host_name = request->GetHeader("HOST").Split(":")[0];
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);
		auto app_config = std::static_pointer_cast<info::Application>(GetApplicationByName(vhost_app_name))->GetConfig();
		auto thumbnail_config = app_config.GetPublishers().GetThumbnailPublisher();

		auto response = client->GetResponse();

		// Check CORS
		auto application = std::static_pointer_cast<ThumbnailApplication>(GetApplicationByName(vhost_app_name));
		if (application == nullptr)
		{
			response->AppendString("There is no application");
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();

			return http::svr::NextHandler::DoNotCall;
		}
		else
		{
			SetAllowOrigin(request->GetUri(), application->GetCorsUrls(), response);
		}

		// Check Stream
		auto stream = std::static_pointer_cast<ThumbnailStream>(GetStream(vhost_app_name, stream_name));
		if (stream == nullptr)
		{
			response->AppendString("There is no stream");
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();

			return http::svr::NextHandler::DoNotCall;
		}

		// Check Extentions
		auto media_codec_id = cmn::MediaCodecId::None;
		if (file_ext.IndexOf("jpg") == 0)
		{
			media_codec_id = cmn::MediaCodecId::Jpeg;
		}
		else if (file_ext.IndexOf("png") == 0)
		{
			media_codec_id = cmn::MediaCodecId::Png;
		}

		// There is no endcoded thumbnail image
		auto endcoded_video_frame = stream->GetVideoFrameByCodecId(media_codec_id);
		if (endcoded_video_frame == nullptr)
		{
			response->AppendString("There is no encoded thumbnail image");
			response->SetStatusCode(http::StatusCode::NotFound);
			response->Response();

			return http::svr::NextHandler::DoNotCall;
		}

		response->SetHeader("Content-Type", (media_codec_id == cmn::MediaCodecId::Jpeg) ? "image/jpeg" : "image/png");
		response->SetStatusCode(http::StatusCode::OK);
		response->AppendData(std::move(endcoded_video_frame->Clone()));
		response->Response();

		return http::svr::NextHandler::DoNotCall;
	});

	return http_interceptor;
}

//====================================================================================================
// ParseRequestUrl
//  ex) ..../app_name/stream_name/file_name.file_ext?param=param_value
//====================================================================================================
bool ThumbnailPublisher::ParseRequestUrl(const ov::String &request_url,
										 ov::String &request_param,
										 ov::String &app_name,
										 ov::String &stream_name,
										 ov::String &file_name,
										 ov::String &file_ext)
{
	ov::String request_path;

	// directory/file.ext?param=test
	auto tokens = request_url.Split("?");
	if (tokens.size() == 0)
	{
		return false;
	}

	request_path = tokens[0];
	request_param = tokens.size() == 2 ? tokens[1] : "";

	// ...../app_name/stream_name/file_name.ext_name
	tokens.clear();
	tokens = request_path.Split("/");

	if (tokens.size() < 3)
	{
		return false;
	}

	app_name = tokens[tokens.size() - 3];
	stream_name = tokens[tokens.size() - 2];
	file_name = tokens[tokens.size() - 1];

	// file_name.ext_name
	tokens.clear();
	tokens = file_name.Split(".");

	if (tokens.size() != 2)
	{
		return false;
	}

	file_ext = tokens[1];

	// logtd(
	// 	"request : %s\n"
	// 	"request path : %s\n"
	// 	"request param : %s\n"
	// 	"app name : %s\n"
	// 	"stream name : %s\n"
	// 	"file name : %s\n"
	// 	"file ext : %s\n",
	// 	request_url.CStr(), request_path.CStr(), request_param.CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr(), file_ext.CStr());

	return true;
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
