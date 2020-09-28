#include <base/ovlibrary/url.h>
#include "file_private.h"
#include "file_publisher.h"

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

	// auto server_config = GetServerConfig();

	// const auto &origin = server_config.GetBind().GetPublishers().GetFile();

	return Publisher::Start();
}

bool FilePublisher::Stop()
{
	return Publisher::Stop();
}

std::shared_ptr<pub::Application> FilePublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return FileApplication::Create(FilePublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool FilePublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto file_application = std::static_pointer_cast<FileApplication>(application);
	if(file_application == nullptr)
	{
		logte("Could not found file application. app:%s", file_application->GetName().CStr());
		return false;
	}

	// File applications and child streams must be terminated.

	return true;
}


bool FilePublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return true;
}

std::shared_ptr<ov::Error> FilePublisher::CommandRecordStart(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	// Find Stream
	auto file_stream = GetStreamAs<FileStream>(vhost_app_name, stream_name);
	if(file_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}

	std::vector<int32_t> dummy_selected_tracks;
	file_stream->RecordStart(dummy_selected_tracks);

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::CommandRecordStop(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	auto file_stream = GetStreamAs<FileStream>(vhost_app_name, stream_name);
	if(file_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}

	file_stream->RecordStop();

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::CommandGetStats(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	// Find Application	
	auto file_stream = GetStreamAs<FileStream>(vhost_app_name, stream_name);
	if(file_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}	

	return ov::Error::CreateError(0, "Success");
}

