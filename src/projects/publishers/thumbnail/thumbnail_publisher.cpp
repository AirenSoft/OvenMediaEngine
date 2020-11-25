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
		logte("An error occurred while creating ThumbnailPublisher");
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
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto http_interceptor = CreateInterceptor();

	bool http_server_result = true;
	ov::SocketAddress address;
	auto &port = thumbnail_bind_config.GetPort();
	if (port.IsParsed())
	{
		address = ov::SocketAddress(server_config.GetIp(), port.GetPort());

		_http_server = manager->CreateHttpServer(address);

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
	auto &tls_port = thumbnail_bind_config.GetTlsPort();
	ov::SocketAddress tls_address;

	if (tls_port.IsParsed())
	{
		const auto &managers = server_config.GetManagers();

		auto host_name_list = std::vector<ov::String>();
		for (auto &name : managers.GetHost().GetNameList())
		{
			host_name_list.push_back(name.GetName());
		}

		tls_address = ov::SocketAddress(server_config.GetIp(), tls_port.GetPort());
		auto certificate = info::Certificate::CreateCertificate("api_server", host_name_list, managers.GetHost().GetTls());

		if (certificate != nullptr)
		{
			_https_server = manager->CreateHttpsServer(address, certificate);

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

	http_interceptor->Register(HttpMethod::Get, R"(.+\.jpg$)", [this](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
		auto request = client->GetRequest();

		ov::String reqeuset_param;
		ov::String app_name;
		ov::String stream_name;
		ov::String file_ext;

		if (ParseRequestUrl(request->GetRequestTarget(), reqeuset_param, app_name, stream_name, file_ext) == false)
		{
			logte("Failed to parse URL: %s", request->GetRequestTarget().CStr());
			return HttpNextHandler::Call;
		}

		auto host_name = request->GetHeader("HOST").Split(":")[0];
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);

		// logtd("host: %s, vshot_app_name: %s", host_name.CStr(), vhost_app_name.CStr());

		auto response = client->GetResponse();

		// There is no stream
		auto stream = std::static_pointer_cast<ThumbnailStream>(GetStream(vhost_app_name, stream_name));
		if (stream == nullptr)
		{
			response->AppendString("There is no stream");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		// There is no endcoded thumbnail image
		auto endcoded_video_frame = stream->GetVideoFrameByCodecId(cmn::MediaCodecId::Jpeg);
		if (endcoded_video_frame == nullptr)
		{
			response->AppendString("There is no encoded thumbnail image");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		response->SetHeader("Content-Type", "image/jpeg");
		response->SetStatusCode(HttpStatusCode::OK);
		response->AppendData(endcoded_video_frame->GetData());
		response->Response();

		return HttpNextHandler::DoNotCall;
	});

	http_interceptor->Register(HttpMethod::Get, R"(.+\.png$)", [this](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
		auto request = client->GetRequest();

		ov::String reqeuset_param;
		ov::String app_name;
		ov::String stream_name;
		ov::String file_ext;

		if (ParseRequestUrl(request->GetRequestTarget(), reqeuset_param, app_name, stream_name, file_ext) == false)
		{
			logte("Failed to parse URL: %s", request->GetRequestTarget().CStr());
			return HttpNextHandler::Call;
		}

		auto host_name = request->GetHeader("HOST").Split(":")[0];
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);

		// logtd("host: %s, vshot_app_name: %s", host_name.CStr(), vhost_app_name.CStr());

		auto response = client->GetResponse();

		// There is no stream
		auto stream = std::static_pointer_cast<ThumbnailStream>(GetStream(vhost_app_name, stream_name));
		if (stream == nullptr)
		{
			response->AppendString("There is no stream");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		// There is no endcoded thumbnail image
		auto endcoded_video_frame = stream->GetVideoFrameByCodecId(cmn::MediaCodecId::Png);
		if (endcoded_video_frame == nullptr)
		{
			response->AppendString("There is no encoded thumbnail image");
			response->SetStatusCode(HttpStatusCode::NotFound);
			response->Response();

			return HttpNextHandler::DoNotCall;
		}

		response->SetHeader("Content-Type", "image/png");
		response->SetStatusCode(HttpStatusCode::OK);
		response->AppendData(endcoded_video_frame->GetData());
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

	if (tokens.size() < 2)
	{
		return false;
	}

	app_name = tokens[tokens.size() - 2];
	auto stream_name_and_ext = tokens[tokens.size() - 1];

	// file_name.ext_name 분리
	tokens.clear();
	tokens = stream_name_and_ext.Split(".");

	if (tokens.size() != 2)
	{
		return false;
	}

	stream_name = tokens[0];
	file_ext = tokens[1];

	// logtd(
	// 	"request : %s\n"
	// 	"request path : %s\n"
	// 	"request param : %s\n"
	// 	"app name : %s\n"
	// 	"stream name : %s\n"
	// 	"file ext : %s\n",
	// 	request_url.CStr(), request_path.CStr(), request_param.CStr(), app_name.CStr(), stream_name.CStr(), file_ext.CStr());

	return true;
}

std::shared_ptr<pub::Application> ThumbnailPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
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
