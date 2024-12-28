//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <orchestrator/orchestrator.h>

#include "base/common_types.h"
#include "base/mediarouter/mediarouter_application_interface.h"
#include "base/ovlibrary/url.h"
#include "base/publisher/publisher.h"
#include "srt_application.h"
#include "srt_session.h"

namespace pub
{
	class SrtPublisher final : public Publisher, public PhysicalPortObserver
	{
	public:
		static std::shared_ptr<SrtPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);

		SrtPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router);
		~SrtPublisher() override final;

		bool Stop() override;

	private:
		bool Start() override;

		std::shared_ptr<ov::Url> GetStreamUrlForRemote(const std::shared_ptr<ov::Socket> &remote, bool *is_vhost_form);

		//--------------------------------------------------------------------
		// Implementation of Publisher
		//--------------------------------------------------------------------
		PublisherType GetPublisherType() const override
		{
			return PublisherType::Srt;
		}
		const char *GetPublisherName() const override
		{
			return "SRTPublisher";
		}

		bool OnCreateHost(const info::Host &host_info) override;
		bool OnDeleteHost(const info::Host &host_info) override;
		std::shared_ptr<Application> OnCreatePublisherApplication(const info::Application &application_info) override;
		bool OnDeletePublisherApplication(const std::shared_ptr<Application> &application) override;
		//--------------------------------------------------------------------

		//--------------------------------------------------------------------
		// Implementation of PhysicalPortObserver
		//--------------------------------------------------------------------
		void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
		void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
		void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;
		//--------------------------------------------------------------------

	private:
		struct StreamMap
		{
			StreamMap(int port, const ov::String &stream_path)
				: port(port),
				  stream_path(stream_path)
			{
			}

			int port;
			ov::String stream_path;
		};

	private:
		std::shared_ptr<StreamMap> GetStreamMap(int port);
		std::shared_ptr<SrtSession> GetSession(const std::shared_ptr<ov::Socket> &remote);

	private:
		std::mutex _physical_port_list_mutex;
		std::vector<std::shared_ptr<PhysicalPort>> _physical_port_list;

		std::shared_mutex _stream_map_mutex;
		// Key: port, Value: StreamMap
		std::map<int, std::shared_ptr<StreamMap>> _stream_map;

		std::shared_mutex _session_map_mutex;
		std::map<session_id_t, std::shared_ptr<SrtSession>> _session_map;
	};
}  // namespace pub
