#include <base/ovlibrary/url.h>
#include "file_private.h"
#include "file_publisher.h"
#include "file_session.h"

std::shared_ptr<FilePublisher> FilePublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto ovt = std::make_shared<FilePublisher>(server_config, router);

	if (!ovt->Start())
	{
		logte("An error occurred while creating FilePublisher");
		return nullptr;
	}

	return ovt;
}

FilePublisher::FilePublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, router)
{
	logtd("FilePublisher has been create");
}

FilePublisher::~FilePublisher()
{
	logtd("FilePublisher has been terminated finally");
}

bool FilePublisher::Start()
{
	logtd("FilePublisher::Start");

	// Listen to localhost:<relay_port>
	//auto server_config = GetServerConfig();

	//const auto &origin = server_config.GetBind().GetPublishers().GetOvt();

	return Publisher::Start();
}

bool FilePublisher::Stop()
{
	return Publisher::Stop();
}

std::shared_ptr<pub::Application> FilePublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	logtd("OnCreatePublisherApplication: %s", application_info.GetName().CStr());

	return FileApplication::Create(FilePublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool FilePublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}


bool FilePublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return true;
}











