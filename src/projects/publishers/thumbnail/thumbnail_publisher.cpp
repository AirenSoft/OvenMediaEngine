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
	auto manager = HttpServerManager::GetInstance();

	auto server_config = GetServerConfig();

	const auto &thumbnail_bind_config = server_config.GetBind().GetPublishers().GetThumbnail();
	if (thumbnail_bind_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto http_interceptor = CreateInterceptor();

	bool http_server_result = true;
	ov::SocketAddress address;
	bool is_parsed;
	auto worker_count = thumbnail_bind_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

	auto &port = thumbnail_bind_config.GetPort(&is_parsed);
	if (is_parsed)
	{
		address = ov::SocketAddress(server_config.GetIp(), port.GetPort());

		_http_server = manager->CreateHttpServer("ThumbnailPublisher", address, worker_count);

		if (_http_server != nullptr)
		{
			_http_server->AddInterceptor(http_interceptor);
		}
		else
		{
			logte("Could not initialize http server");
			http_server_result = false;
		}
	}

	bool https_server_result = true;
	auto &tls_port = thumbnail_bind_config.GetTlsPort(&is_parsed);
	ov::SocketAddress tls_address;

	if (is_parsed)
	{
		const auto &managers = server_config.GetManagers();

		auto host_name_list = std::vector<ov::String>();
		for (auto &name : managers.GetHost().GetNameList())
		{
			host_name_list.push_back(name);
		}

		tls_address = ov::SocketAddress(server_config.GetIp(), tls_port.GetPort());
		auto certificate = info::Certificate::CreateCertificate("api_server", host_name_list, managers.GetHost().GetTls());

		if (certificate != nullptr)
		{
			_https_server = manager->CreateHttpsServer("ThumbnailPublisher", tls_address, certificate);

			if (_https_server != nullptr)
			{
				_https_server->AddInterceptor(http_interceptor);
			}
			else
			{
				logte("Could not initialize thumbnail https server");
				https_server_result = false;
			}
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
		logti("Thumbnail publisher is listening on %s", address.ToString().CStr());
	}

	return Publisher::Start();
}

bool ThumbnailPublisher::Stop()
{
	auto manager = HttpServerManager::GetInstance();

	std::shared_ptr<HttpServer> http_server = std::move(_http_server);
	std::shared_ptr<HttpsServer> https_server = std::move(_https_server);

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

std::shared_ptr<HttpRequestInterceptor> ThumbnailPublisher::CreateInterceptor()
{
	auto http_interceptor = std::make_shared<HttpDefaultInterceptor>();

	http_interceptor->Register(HttpMethod::Get, R"(.+thumb\.(jpg|png)$)", [this](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
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
			return HttpNextHandler::Call;
		}

		auto host_name = request->GetHeader("HOST").Split(":")[0];
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);

		auto response = client->GetResponse();

		// Check Stream
		auto stream = std::static_pointer_cast<ThumbnailStream>(GetStream(vhost_app_name, stream_name));
		if (stream == nullptr)
		{
			response->AppendString("There is no stream");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		/*
		// Check Filename
		if (file_name.IndexOf("thumb") != 0)
		{
			// This part is not excute due to the regular expression.
			response->AppendString("Thre is no file");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}
		*/

		// check Extentions
		auto media_codec_id = cmn::MediaCodecId::None;
		if (file_ext.IndexOf("jpg") == 0)
		{
			media_codec_id = cmn::MediaCodecId::Jpeg;
		}
		else if (file_ext.IndexOf("png") == 0)
		{
			media_codec_id = cmn::MediaCodecId::Png;
		}
		/*
		else
		{
			// This part is not excute due to the regular expression.
			response->AppendString("Thre is no file(invalid extention)");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}
		*/

		// There is no endcoded thumbnail image
		auto endcoded_video_frame = stream->GetVideoFrameByCodecId(media_codec_id);
		if (endcoded_video_frame == nullptr)
		{
			response->AppendString("There is no encoded thumbnail image");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		response->SetHeader("Content-Type", (media_codec_id == cmn::MediaCodecId::Jpeg) ? "image/jpeg" : "image/png");
		response->SetStatusCode(HttpStatusCode::OK);
		response->AppendData(std::move(endcoded_video_frame->Clone()));
		response->Response();

		return HttpNextHandler::DoNotCall;
	});

	return http_interceptor;
}

//====================================================================================================
// ParseRequestUrl
// - URL 분리
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

std::shared_ptr<pub::Application> ThumbnailPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if(IsModuleAvailable() == false)
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
