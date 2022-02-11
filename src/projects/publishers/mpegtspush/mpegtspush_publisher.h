#pragma once

#include <orchestrator/orchestrator.h>

#include "base/common_types.h"
#include "base/info/push.h"
#include "base/mediarouter/mediarouter_application_interface.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "mpegtspush_application.h"
#include "mpegtspush_userdata.h"

class MpegtsPushPublisher : public pub::Publisher
{
public:
	static std::shared_ptr<MpegtsPushPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	MpegtsPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~MpegtsPushPublisher() override;
	bool Stop() override;

private:
	bool Start() override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	PublisherType GetPublisherType() const override
	{
		return PublisherType::MpegtsPush;
	}
	const char *GetPublisherName() const override
	{
		return "MPEGTSPushPublisher";
	}

	bool OnCreateHost(const info::Host &host_info) override;
	bool OnDeleteHost(const info::Host &host_info) override;
	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;
};
