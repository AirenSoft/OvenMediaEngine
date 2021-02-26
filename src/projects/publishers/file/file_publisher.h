#pragma once

#include "base/common_types.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"

#include "base/info/record.h"
#include "file_application.h"
#include "file_userdata.h"

#include <orchestrator/orchestrator.h>

class FilePublisher : public pub::Publisher
{
public:
	static std::shared_ptr<FilePublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	FilePublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~FilePublisher() override;
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
		return "FilePublisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

private:
	void SessionController();
	void StartSession(std::shared_ptr<FileSession> session);
	void StopSession(std::shared_ptr<FileSession> session);
	void SplitSession(std::shared_ptr<FileSession> session);
	
	void WorkerThread();
	bool _stop_thread_flag;
	std::thread _worker_thread;

	std::shared_mutex _userdata_sets_mutex;;
	FileUserdataSets _userdata_sets;

public:
	enum FilePublisherStatusCode {
		Success,
		FailureInvalidParameter,
		FailureDupulicateKey,
		FailureNotExist,
		FailureUnknown
	};
	
	std::shared_ptr<ov::Error> RecordStart(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> record);
	std::shared_ptr<ov::Error> RecordStop(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> record);
	std::shared_ptr<ov::Error> GetRecords(const info::VHostAppName &vhost_app_name, std::vector<std::shared_ptr<info::Record>> &record_list);

};
