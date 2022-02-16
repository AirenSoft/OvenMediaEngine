#pragma once

#include <modules/http/server/http_server_manager.h>
#include <orchestrator/orchestrator.h>

#include "base/common_types.h"
#include "base/info/record.h"
#include "base/mediarouter/mediarouter_application_interface.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "thumbnail_application.h"
#include "thumbnail_interceptor.h"

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

	bool OnCreateHost(const info::Host &host_info) override;
	bool OnDeleteHost(const info::Host &host_info) override;
	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

	bool SetAllowOrigin(const ov::String &origin_url, std::vector<ov::String>& cors_urls, const std::shared_ptr<http::svr::HttpResponse> &response);

private:
	std::shared_ptr<ThumbnailInterceptor> CreateInterceptor();
	std::shared_ptr<http::svr::HttpServer> _http_server;
	std::shared_ptr<http::svr::HttpsServer> _https_server;
};
