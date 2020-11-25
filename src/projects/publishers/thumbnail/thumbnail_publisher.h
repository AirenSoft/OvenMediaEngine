#pragma once

#include <modules/http_server/http_server_manager.h>
#include <orchestrator/orchestrator.h>

#include "base/common_types.h"
#include "base/info/record.h"
#include "base/mediarouter/media_route_application_interface.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "thumbnail_application.h"

class ThumbnailPublisher : public pub::Publisher
{
public:
	static std::shared_ptr<ThumbnailPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	ThumbnailPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~ThumbnailPublisher() override;
	bool Stop() override;

private:
	bool Start() override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	PublisherType GetPublisherType() const override
	{
		return PublisherType::Thumbnail;
	}
	const char *GetPublisherName() const override
	{
		return "ThumbnailPublisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

	bool ParseRequestUrl(const ov::String &request_url,
						 ov::String &request_param,
						 ov::String &app_name,
						 ov::String &stream_name,
						 ov::String &file_name,
						 ov::String &file_ext);

private:
	std::shared_ptr<HttpRequestInterceptor> CreateInterceptor();
	std::shared_ptr<HttpServer> _http_server;
	std::shared_ptr<HttpsServer> _https_server;
};
