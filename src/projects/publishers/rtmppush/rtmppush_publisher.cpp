#include "rtmppush_publisher.h"

#include "rtmppush_private.h"

#define UNUSED(expr)  \
	do                \
	{                 \
		(void)(expr); \
	} while (0)

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
	return Publisher::Start();
}

bool RtmpPushPublisher::Stop()
{
	return Publisher::Stop();
}

bool RtmpPushPublisher::OnCreateHost(const info::Host &host_info)
{
	return true;
}

bool RtmpPushPublisher::OnDeleteHost(const info::Host &host_info)
{
	return true;
}

std::shared_ptr<pub::Application> RtmpPushPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return RtmpPushApplication::Create(RtmpPushPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool RtmpPushPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto rtmppush_application = std::static_pointer_cast<RtmpPushApplication>(application);
	if (rtmppush_application == nullptr)
	{
		logte("Could not found file application. app:%s", rtmppush_application->GetName().CStr());
		return false;
	}

	// Applications and child streams must be terminated.

	return true;
}
