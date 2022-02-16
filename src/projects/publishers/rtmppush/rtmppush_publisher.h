#pragma once

#include <orchestrator/orchestrator.h>

#include "base/common_types.h"
#include "base/info/push.h"
#include "base/mediarouter/mediarouter_application_interface.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "rtmppush_application.h"
#include "rtmppush_userdata.h"

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
		return "RTMPPushPublisher";
	}

	bool OnCreateHost(const info::Host &host_info) override;
	bool OnDeleteHost(const info::Host &host_info) override;
	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;
};
