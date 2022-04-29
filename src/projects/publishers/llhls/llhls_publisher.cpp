//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include <base/ovlibrary/url.h>

#include "llhls_publisher.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsPublisher> LLHlsPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto llhls = std::make_shared<LLHlsPublisher>(server_config, router);
	if (!llhls->Start())
	{
		return nullptr;
	}

	return llhls;
}

LLHlsPublisher::LLHlsPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
	logtd("LLHlsPublisher has been create");
}

LLHlsPublisher::~LLHlsPublisher()
{
	logtd("LLHlsPublisher has been terminated finally");
}

bool LLHlsPublisher::Start()
{
	auto server_config = GetServerConfig();

	auto llhls_module_config = server_config.GetModules().GetLLHls();
	if (llhls_module_config.IsEnabled() == false)
	{
		logtw("LLHls Module is disabled");
		return true;
	}

	auto llhls_bind_config = server_config.GetBind().GetPublishers().GetLLHls();

	if (llhls_bind_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	return Publisher::Start();
}

bool LLHlsPublisher::Stop()
{
	return Publisher::Stop();
}

bool LLHlsPublisher::OnCreateHost(const info::Host &host_info)
{
	return true;
}

bool LLHlsPublisher::OnDeleteHost(const info::Host &host_info)
{
	return true;
}

std::shared_ptr<pub::Application> LLHlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return LLHlsApplication::Create(LLHlsPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool LLHlsPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto llhls_application = std::static_pointer_cast<LLHlsApplication>(application);
	if (llhls_application == nullptr)
	{
		logte("Could not found llhls application. app:%s", llhls_application->GetName().CStr());
		return false;
	}

	return true;
}