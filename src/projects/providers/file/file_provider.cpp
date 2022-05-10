//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================


#include "file_provider.h"

#include <base/ovlibrary/url.h>

#include "file_application.h"
#include "file_private.h"
#include "file_stream.h"

using namespace cmn;

namespace pvd
{
	std::shared_ptr<FileProvider> FileProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<FileProvider>(server_config, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating File Provider");
			return nullptr;
		}
		return provider;
	}

	FileProvider::FileProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: PullProvider(server_config, router)
	{
		logtd("Created File Provider module.");
	}

	FileProvider::~FileProvider()
	{
		Stop();

		logtd("Terminated File Provider module.");
	}

	bool FileProvider::OnCreateHost(const info::Host &host_info)
	{
		return true;
	}

	bool FileProvider::OnDeleteHost(const info::Host &host_info)
	{
		return true;
	}

	std::shared_ptr<pvd::Application> FileProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		if (IsModuleAvailable() == false)
		{
			return nullptr;
		}

		bool is_parsed = false;
		app_info.GetConfig().GetProviders().GetFileProvider(&is_parsed);
		if (!is_parsed)
		{
			return nullptr;
		}

		return FileApplication::Create(GetSharedPtrAs<pvd::PullProvider>(), app_info);
	}

	bool FileProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return true;
	}
}  // namespace pvd