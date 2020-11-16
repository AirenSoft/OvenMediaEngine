//
// Created by getroot on 19. 12. 9.
//
#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/provider/pull_provider/application.h>
#include <base/provider/pull_provider/provider.h>
#include <orchestrator/orchestrator.h>

/*
 * RtspcProvider
 * 		: Create PhysicalPort, OvtApplication
 *
 * OvtApplication
 * 		: Create MediaRouteApplicationConnector, OvtStream
 *
 * OvtStream
 * 		: Create by interface (PullStream)
 * 		: Create Thread, the Thread has a Queue
 * 				: Communicate with OvtPublisher of Origin Server
 * 				: Send packets by RtspcProvider::PhysicalPort
 * 				: Receive packets from RtspcProvider -> OvtApplication -> OvtStream -> Queue
 *
 */

namespace pvd
{
	class RtspcProvider : public pvd::PullProvider
	{

	public:
		static std::shared_ptr<RtspcProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

		explicit RtspcProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

		~RtspcProvider() override;

		ProviderStreamDirection GetProviderStreamDirection() const override
		{
			return ProviderStreamDirection::Pull;
		}

		ProviderType GetProviderType() const override
		{
			return ProviderType::RtspPull;
		}
		const char* GetProviderName() const override
	    {
		    return "RTSPCProvider";
	    }

	protected:
		std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &app_info) override;
		bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;
	};
}  // namespace pvd