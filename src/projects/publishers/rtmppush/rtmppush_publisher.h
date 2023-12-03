#pragma once


#include "base/info/push.h"
#include "base/publisher/publisher.h"

class RtmpPushPublisher : public pub::Publisher
{
public:
	static std::shared_ptr<RtmpPushPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);

	RtmpPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);
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
