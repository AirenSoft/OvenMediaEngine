#include "thumbnail_application.h"

#include "thumbnail_private.h"
#include "thumbnail_stream.h"

std::shared_ptr<ThumbnailApplication> ThumbnailApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<ThumbnailApplication>(publisher, application_info);
	application->Start();
	return application;
}

ThumbnailApplication::ThumbnailApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{
	auto thumbnail_config = application_info.GetConfig().GetPublishers().GetThumbnailPublisher();

	SetCrossDomain(thumbnail_config.GetCrossDomainList());
}

ThumbnailApplication::~ThumbnailApplication()
{
	Stop();
	logtd("ThumbnailApplication(%d) has been terminated finally", GetId());
}

bool ThumbnailApplication::Start()
{
	return Application::Start();
}

bool ThumbnailApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<pub::Stream> ThumbnailApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("Created stream : %s/%u", info->GetName().CStr(), info->GetId());

	return ThumbnailStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool ThumbnailApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("ThumbnailApplication::DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<ThumbnailStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("ThumbnailApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	logtd("ThumbnailApplication %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

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
//
// @Refer to segment_stream_server.cpp
//====================================================================================================
void ThumbnailApplication::SetCrossDomain(const std::vector<ov::String> &url_list)
{
	std::vector<ov::String> crossdmain_urls;
	ov::String http_prefix = "http://";
	ov::String https_prefix = "https://";

	if (url_list.empty())
	{
		return;
	}

	for (auto &url : url_list)
	{
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

	ov::String cors_urls;
	for (auto &url : _cors_urls)
		cors_urls += url + "\n";
	logtd("CORS \n%s", cors_urls.CStr());
}

bool ThumbnailApplication::UrlExistCheck(const std::vector<ov::String> &url_list, const ov::String &check_url)
{
	auto item = std::find_if(url_list.begin(), url_list.end(),
							 [&check_url](auto &url) -> bool {
								 return check_url == url;
							 });

	return (item != url_list.end());
}

std::vector<ov::String> &ThumbnailApplication::GetCorsUrls()
{
	return _cors_urls;
}