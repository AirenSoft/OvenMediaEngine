#pragma once

#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"

#include "rtmppush_application.h"

#include <orchestrator/orchestrator.h>

class RtmpPushPublisher : public pub::Publisher
{
public:
	static std::shared_ptr<RtmpPushPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	RtmpPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~RtmpPushPublisher() override;
	bool Stop() override;

private:
	bool Start() override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	PublisherType GetPublisherType() const override
	{
		return PublisherType::RtmpPush;
	}
	const char *GetPublisherName() const override
	{
		return "RtmpPushPublisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;
	
	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections) override;

public:
	std::shared_ptr<ov::Error> HandlePushCreate(const info::VHostAppName &vhost_app_name, ov::String stream_name);
	std::shared_ptr<ov::Error> HandlePushUpdate(const info::VHostAppName &vhost_app_name, ov::String stream_name);
	std::shared_ptr<ov::Error> HandlePushRead(const info::VHostAppName &vhost_app_name, ov::String stream_name);
	std::shared_ptr<ov::Error> HandlePushDelete(const info::VHostAppName &vhost_app_name, ov::String stream_name);

};
