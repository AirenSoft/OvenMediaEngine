#pragma once

#include "base/common_types.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"

#include "base/info/push.h"
#include "rtmppush_application.h"
#include "rtmppush_userdata.h"

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
		return "RTMPPushPublisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

private:
	void SessionController();
	void StartSession(std::shared_ptr<RtmpPushSession> session);
	void StopSession(std::shared_ptr<RtmpPushSession> session);
	
	void WorkerThread();
	bool _stop_thread_flag;
	std::thread _worker_thread;

	std::shared_mutex _userdata_sets_mutex;;
	RtmpPushUserdataSets _userdata_sets;

public:
	enum PushPublisherErrorCode {
		Success,
		FailureInvalidParameter,
		FailureDupulicateKey,
		FailureNotExist,
		FailureUnknown
	};
	
	std::shared_ptr<ov::Error> PushStart(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Push> &record);
	std::shared_ptr<ov::Error> PushStop(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Push> &record);
	std::shared_ptr<ov::Error> GetPushes(const info::VHostAppName &vhost_app_name, std::vector<std::shared_ptr<info::Push>> &record_list);
};
