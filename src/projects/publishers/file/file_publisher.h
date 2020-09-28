#pragma once

#include "modules/ovt_packetizer/ovt_packet.h"

#include "base/common_types.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"

#include "file_application.h"

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
	
	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections) override;


public:
	std::shared_ptr<ov::Error> CommandRecordStart(const info::VHostAppName &vhost_app_name, ov::String stream_name);
	std::shared_ptr<ov::Error> CommandRecordStop(const info::VHostAppName &vhost_app_name, ov::String stream_name);
	std::shared_ptr<ov::Error> CommandGetStats(const info::VHostAppName &vhost_app_name, ov::String stream_name);

};
