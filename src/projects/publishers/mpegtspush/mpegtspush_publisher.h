#pragma once


#include "base/info/push.h"
#include "base/publisher/publisher.h"

class MpegtsPushPublisher : public pub::Publisher
{
public:
	static std::shared_ptr<MpegtsPushPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);

	MpegtsPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);
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
