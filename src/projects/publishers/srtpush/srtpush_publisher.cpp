//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srtpush_publisher.h"
#include "srtpush_application.h"
#include "srtpush_private.h"

#define UNUSED(expr)  \
	do                \
	{                 \
		(void)(expr); \
	} while (0)

std::shared_ptr<SrtPushPublisher> SrtPushPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
{
	auto obj = std::make_shared<SrtPushPublisher>(server_config, router);

	if (!obj->Start())
	{
		logte("An error occurred while creating SrtPushPublisher");
		return nullptr;
	}

	return obj;
}

SrtPushPublisher::SrtPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
	: Publisher(server_config, router)
{
	logtd("SrtPushPublisher has been create");
}

SrtPushPublisher::~SrtPushPublisher()
{
	logtd("SrtPushPublisher has been terminated finally");
}

bool SrtPushPublisher::Start()
{
	return Publisher::Start();
}

bool SrtPushPublisher::Stop()
{
	return Publisher::Stop();
}

bool SrtPushPublisher::OnCreateHost(const info::Host &host_info)
{
	return true;
}

bool SrtPushPublisher::OnDeleteHost(const info::Host &host_info)
{
	return true;
}

std::shared_ptr<pub::Application> SrtPushPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return SrtPushApplication::Create(SrtPushPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool SrtPushPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto push_application = std::static_pointer_cast<SrtPushApplication>(application);
	if (push_application == nullptr)
	{
		logte("Could not found file application. app:%s", push_application->GetName().CStr());
		return false;
	}

	// Applications and child streams must be terminated.

	return true;
}