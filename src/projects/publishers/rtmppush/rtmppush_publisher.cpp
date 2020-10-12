#include "rtmppush_private.h"
#include "rtmppush_publisher.h"

std::shared_ptr<RtmpPushPublisher> RtmpPushPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto obj = std::make_shared<RtmpPushPublisher>(server_config, router);

	if (!obj->Start())
	{
		logte("An error occurred while creating RtmpPushPublisher");
		return nullptr;
	}

	return obj;
}

RtmpPushPublisher::RtmpPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, router)
{
	logtd("RtmpPushPublisher has been create");
}

RtmpPushPublisher::~RtmpPushPublisher()
{
	logtd("RtmpPushPublisher has been terminated finally");
}

bool RtmpPushPublisher::Start()
{
	logtd("RtmpPushPublisher::Start");

	// auto server_config = GetServerConfig();

	// const auto &origin = server_config.GetBind().GetPublishers().GetFile();

	return Publisher::Start();
}

bool RtmpPushPublisher::Stop()
{
	return Publisher::Stop();
}

std::shared_ptr<pub::Application> RtmpPushPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return RtmpPushApplication::Create(RtmpPushPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool RtmpPushPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto rtmppush_application = std::static_pointer_cast<RtmpPushApplication>(application);
	if(rtmppush_application == nullptr)
	{
		logte("Could not found file application. app:%s", rtmppush_application->GetName().CStr());
		return false;
	}

	// File applications and child streams must be terminated.

	return true;
}


bool RtmpPushPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return true;
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushCreate(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	// Find Stream
	auto rtmppush_stream = GetStreamAs<RtmpPushStream>(vhost_app_name, stream_name);
	if(rtmppush_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}

	// std::vector<int32_t> dummy_selected_tracks;
	// rtmppush_stream->RecordStart(dummy_selected_tracks);

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushUpdate(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	auto rtmppush_stream = GetStreamAs<RtmpPushStream>(vhost_app_name, stream_name);
	if(rtmppush_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}

	// rtmppush_stream->RecordStop();

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushRead(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	// Find Application	
	auto rtmppush_stream = GetStreamAs<RtmpPushStream>(vhost_app_name, stream_name);
	if(rtmppush_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}	

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushDelete(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	// Find Application	
	auto rtmppush_stream = GetStreamAs<RtmpPushStream>(vhost_app_name, stream_name);
	if(rtmppush_stream == nullptr)
	{
		logte("Could not found file stream. app:%s, stream:%s", vhost_app_name.CStr(), stream_name.CStr());
		return 	ov::Error::CreateError(0, "Failed");
	}	

	return ov::Error::CreateError(0, "Success");
}

