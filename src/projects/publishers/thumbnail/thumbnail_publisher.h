#pragma once

#include "base/common_types.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"

#include "base/info/record.h"
#include "thumbnail_application.h"
#include "thumbnail_userdata.h"

#include <orchestrator/orchestrator.h>

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
		return PublisherType::File;
	}
	const char *GetPublisherName() const override
	{
		return "ThumbnailPublisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

private:
	void SessionController();
	void StartSession(std::shared_ptr<ThumbnailSession> session);
	void StopSession(std::shared_ptr<ThumbnailSession> session);
	
	void WorkerThread();
	bool _stop_thread_flag;
	std::thread _worker_thread;

	std::shared_mutex _userdata_sets_mutex;;
	ThumbnailUserdataSets _userdata_sets;

public:
	enum ThumbnailPublisherStatusCode {
		Success,
		Failure
	};
	
	std::shared_ptr<ov::Error> RecordStart(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> &record);
	std::shared_ptr<ov::Error> RecordStop(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> &record);
	std::shared_ptr<ov::Error> GetRecords(const info::VHostAppName &vhost_app_name, std::vector<std::shared_ptr<info::Record>> &record_list);

};
