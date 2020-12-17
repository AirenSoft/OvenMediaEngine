//==============================================================================
//
//  MPEGTS Provider
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
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
	class MpegTsStreamPortItem
	{
	public:
		// State : Init | Bound, Attached | Detached, Connected | Disconnected
		MpegTsStreamPortItem(ov::SocketType scheme, uint16_t port, const std::shared_ptr<PhysicalPort> &physical_port)
			: _scheme(scheme),
			  _port(port),
			  _physical_port(physical_port)
		{
		}

		ov::SocketType GetScheme()
		{
			return _scheme;
		}

		uint16_t GetPortNumber()
		{
			return _port;
		}

		const info::VHostAppName &GetVhostAppName()
		{
			return _vhost_app_name;
		}

		const ov::String &GetOutputStreamName() const
		{
			return _stream_name;
		}

		const std::shared_ptr<PhysicalPort> &GetPhysicalPort()
		{
			return _physical_port;
		}

		void AttachToApplication(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
		{
			_attached = true;
			_vhost_app_name = vhost_app_name;
			_stream_name = stream_name;
		}

		void OnClientConnected(uint32_t client_id)
		{
			_client_connected = true;
			_client_id = client_id;
		}

		void DetachFromApplication()
		{
			_attached = false;
			OnClientDisconnected();
		}

		void OnClientDisconnected()
		{
			_client_connected = false;
		}

		bool IsAttached()
		{
			return _attached.load();
		}

		bool IsClientConnected()
		{
			return _client_connected.load();
		}

		uint32_t GetClientId()
		{
			return _client_id.load();
		}

	private:
		ov::SocketType _scheme = ov::SocketType::Udp;
		uint16_t _port = 0;
		info::VHostAppName _vhost_app_name = info::VHostAppName::InvalidVHostAppName();
		ov::String _stream_name;
		std::shared_ptr<PhysicalPort> _physical_port;

		std::atomic<bool> _attached = false;
		std::atomic<bool> _client_connected = false;
		std::atomic<uint32_t> _client_id = 0;
	};

	class MpegTsProvider : public pvd::PushProvider, protected PhysicalPortObserver
	{
	public:
		static std::shared_ptr<MpegTsProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

		explicit MpegTsProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		~MpegTsProvider() override;

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
			return ProviderType::Mpegts;
		}

		const char *GetProviderName() const override
		{
			return "MPEGTSProvider";
		}

	protected:
		struct StreamInfo
		{
			StreamInfo(info::VHostAppName vhost_app_name, ov::String stream_name)
				: vhost_app_name(std::move(vhost_app_name)),
				  stream_name(std::move(stream_name))
			{
			}
			
			info::VHostAppName vhost_app_name;
			ov::String stream_name;
		};

		bool BindMpegTSPorts();

		// stream_map->key: <port, type>
		// stream_map->value: <vhost_app_name, stream_name>
		bool PrepareStreamList(const cfg::Server &server_config, std::map<std::tuple<int, ov::SocketType>, StreamInfo> *stream_map);

		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
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
		std::shared_ptr<MpegTsStreamPortItem> GetStreamPortItem(uint16_t local_port);
		std::shared_ptr<MpegTsStreamPortItem> GetDetachedStreamPortItem();

		std::shared_mutex _stream_port_map_lock;
		std::map<uint16_t, std::shared_ptr<MpegTsStreamPortItem>> _stream_port_map;
	};
}  // namespace pvd