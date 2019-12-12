//
// Created by getroot on 19. 12. 9.
//
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/provider/provider.h>
#include <base/provider/application.h>
#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>


/*
 * OvtProvider
 * 		: Create PhysicalPort, OvtApplication
 *
 * OvtApplication
 * 		: Create MediaRouteApplicationConnector, OvtStream
 *
 * OvtStream
 * 		: Create by interface (PullStream)
 * 		: Create Thread, the Thread has a Queue
 * 				: Communicate with OvtPublisher of Origin Server
 * 				: Send packets by OvtProvider::PhysicalPort
 * 				: Receive packets from OvtProvider -> OvtApplication -> OvtStream -> Queue
 *
 */


class OvtProvider : public pvd::Provider
{
public:
	static std::shared_ptr<OvtProvider> Create(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);

	explicit OvtProvider(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);
	~OvtProvider() override;

	ProviderType GetProviderType() override
	{
		return ProviderType::Ovt;
	}

	bool Start() override;
	bool Stop() override;

	bool PullStream(ov::String url);


protected:
	std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &app_info) override;
	bool OnDeleteProviderApplication(const info::Application &app_info) override;
};
