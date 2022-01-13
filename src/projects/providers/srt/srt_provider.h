//==============================================================================
//
//  SRT Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>
#include <orchestrator/orchestrator.h>
#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

#include "base/provider/push_provider/provider.h"

namespace pvd
{
	class SrtProvider : public pvd::PushProvider, protected PhysicalPortObserver
	{
	public:
		static std::shared_ptr<SrtProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

		explicit SrtProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		~SrtProvider() override;

		bool Start() override;
		bool Stop() override;

		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
		ProviderStreamDirection GetProviderStreamDirection() const override
		{
			return ProviderStreamDirection::Push;
		}

		ProviderType GetProviderType() const override
		{
			return ProviderType::Srt;
		}

		const char *GetProviderName() const override
		{
			return "SrtProvider";
		}

	protected:
		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
		bool OnCreateHost(const info::Host &host_info) override;
		bool OnDeleteHost(const info::Host &host_info) override;
		std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &application_info) override;
		bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;

		//--------------------------------------------------------------------
		// Implementation of PushProvider's virtual functions
		//--------------------------------------------------------------------
		void OnTimer(const std::shared_ptr<PushStream> &channel) override;

		//--------------------------------------------------------------------
		// Implementation of PhysicalPortObserver
		//--------------------------------------------------------------------
		void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;

		void OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
							const ov::SocketAddress &address,
							const std::shared_ptr<const ov::Data> &data) override;

		void OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
							PhysicalPortDisconnectReason reason,
							const std::shared_ptr<const ov::Error> &error) override;

	private:
		std::shared_ptr<PhysicalPort> _physical_port;
	};
}  // namespace pvd