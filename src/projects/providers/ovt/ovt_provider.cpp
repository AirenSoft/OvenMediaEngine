//
// Created by getroot on 19. 12. 9.
//

#include <base/ovlibrary/url.h>
#include "ovt_provider.h"
#include "ovt_application.h"
#include "ovt_stream.h"

#define OV_LOG_TAG "OvtProvider"

using namespace cmn;

namespace pvd
{
	std::shared_ptr<OvtProvider> OvtProvider::Create(const cfg::Server &server_config,
													 const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<OvtProvider>(server_config, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating OvtProvider");
			return nullptr;
		}

		return provider;
	}

	OvtProvider::OvtProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
			: PullProvider(server_config, router)
	{
		auto &ovt_provider_config = server_config.GetBind().GetProviders().GetOvt();

		if (ovt_provider_config.IsParsed())
		{
			bool is_parsed;
			auto worker_count = ovt_provider_config.GetWorkerCount(&is_parsed);
			worker_count = is_parsed ? worker_count : PHYSICAL_PORT_DEFAULT_WORKER_COUNT;

			_client_socket_pool = ov::SocketPool::Create("OvtProvider", ov::SocketType::Tcp);

			_client_socket_pool->Initialize(worker_count);
		}
	}

	OvtProvider::~OvtProvider()
	{
		Stop();
		logtd("Terminated OvtProvider modules.");
	}

	std::shared_ptr<pvd::Application> OvtProvider::OnCreateProviderApplication(const info::Application &app_info)
	{
		return OvtApplication::Create(GetSharedPtrAs<pvd::PullProvider>(), app_info);
	}

	bool OvtProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return true; 
	}
}