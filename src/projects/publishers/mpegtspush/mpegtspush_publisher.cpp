#include "mpegtspush_publisher.h"

#include "mpegtspush_private.h"

#define UNUSED(expr)  \
	do                \
	{                 \
		(void)(expr); \
	} while (0)

std::shared_ptr<MpegtsPushPublisher> MpegtsPushPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto obj = std::make_shared<MpegtsPushPublisher>(server_config, router);

	if (!obj->Start())
	{
		logte("An error occurred while creating MpegtsPushPublisher");
		return nullptr;
	}

	return obj;
}

MpegtsPushPublisher::MpegtsPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
	logtd("MpegtsPushPublisher has been create");
}

MpegtsPushPublisher::~MpegtsPushPublisher()
{
	logtd("MpegtsPushPublisher has been terminated finally");
}

bool MpegtsPushPublisher::Start()
{
	return Publisher::Start();
}

bool MpegtsPushPublisher::Stop()
{
	return Publisher::Stop();
}

bool MpegtsPushPublisher::OnCreateHost(const info::Host &host_info)
{
	return true;
}

bool MpegtsPushPublisher::OnDeleteHost(const info::Host &host_info)
{
	return true;
}

std::shared_ptr<pub::Application> MpegtsPushPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return MpegtsPushApplication::Create(MpegtsPushPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool MpegtsPushPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto mpegtspush_application = std::static_pointer_cast<MpegtsPushApplication>(application);
	if (mpegtspush_application == nullptr)
	{
		logte("Could not found file application. app:%s", mpegtspush_application->GetName().CStr());
		return false;
	}

	// Applications and child streams must be terminated.

	return true;
}
