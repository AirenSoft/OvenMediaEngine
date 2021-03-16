//==============================================================================
//
//  MPEGTS Provider
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_provider.h"

#include <modules/physical_port/physical_port_manager.h>
#include <orchestrator/orchestrator.h>

#include "mpegts_application.h"
#include "mpegts_provider_private.h"
#include "mpegts_stream.h"

namespace pvd
{
	std::shared_ptr<MpegTsProvider> MpegTsProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<MpegTsProvider>(server_config, router);
		if (!provider->Start())
		{
			return nullptr;
		}
		return provider;
	}

	MpegTsProvider::MpegTsProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: PushProvider(server_config, router)
	{
		logtd("Created Mpegts Provider module.");
	}

	MpegTsProvider::~MpegTsProvider()
	{
		logti("Terminated Mpegts Provider module.");
	}

	bool MpegTsProvider::PrepareStreamList(const cfg::Server &server_config, std::map<std::tuple<int, ov::SocketType>, StreamInfo> *stream_map)
	{
		// Build a port-stream map from the configuration
		//
		// <Server>
		// 	<VirtualHosts>
		// 		<Applications>
		// 			<Application>
		// 				<Providers>
		// 				<MPEGTS>
		// 					<StreamMap>
		// 						<Stream>
		//							<Name>mpegts_test_stream</Name>
		// 							<OutputStreamName>stream_${Port}</OutputStreamName>
		// 							<Port>40000-40001,40004,40005/udp</Port>
		// 						</Stream>
		// 					</StreamMap>
		// 				</MPEGTS>
		// ...
		auto &mpegts_provider_config = server_config.GetBind().GetProviders().GetMpegts();
		auto &port_config = mpegts_provider_config.GetPort();
		auto &port_list_config = port_config.GetPortList();
		auto &vhost_list_config = server_config.GetVirtualHostList();

		for (auto &vhost_config : vhost_list_config)
		{
			auto app_list_config = vhost_config.GetApplicationList();

			for (auto &app_config : app_list_config)
			{
				auto &mpegts_provider_config = app_config.GetProviders().GetMpegtsProvider();
				auto &stream_map_config = mpegts_provider_config.GetStreamMap();
				auto &stream_list_config = stream_map_config.GetStreamList();

				for (auto &stream_config : stream_list_config)
				{
					auto stream_port_config = stream_config.GetPort();
					auto port_list = stream_port_config.GetPortList();

					for (auto port : port_list)
					{
						{
							// Ensure that a port not speficied in <Bind> is used
							auto found = std::find(port_list_config.begin(), port_list_config.end(), port);

							if (found == port_list_config.end())
							{
								logte("Port not listed in <Bind> was used: %d", port);
								return false;
							}
						}

						auto key = std::make_tuple(port, port_config.GetSocketType());

						{
							// Check if it conflicts with other settings
							auto found = stream_map->find(key);
							if (found != stream_map->end())
							{
								auto value = found->second;

								logte("%d port is already in use: %s/%s", port, value.vhost_app_name.CStr(), value.stream_name.CStr());
								return false;
							}
						}

						auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationName(vhost_config.GetName(), app_config.GetName());
						auto stream_name = stream_config.GetName().Replace("${Port}", ov::Converter::ToString(port));

						stream_map->emplace(std::move(key), StreamInfo(vhost_app_name, stream_name));
					}
				}
			}
		}

		return true;
	}

	bool MpegTsProvider::BindMpegTSPorts()
	{
		auto &server_config = GetServerConfig();
		auto &ip = server_config.GetIp();
		auto &mpegts_provider_config = server_config.GetBind().GetProviders().GetMpegts();
		auto &port_config = mpegts_provider_config.GetPort();
		auto &port_list_config = port_config.GetPortList();
		auto socket_type = port_config.GetSocketType();

		for (const auto &port : port_list_config)
		{
			auto address = ov::SocketAddress(ip, port);
			auto physical_port = PhysicalPortManager::GetInstance()->CreatePort("MPEGTS", socket_type, address, 1);
			if (physical_port == nullptr)
			{
				logte("Could not initialize phyiscal port for MPEG-TS server: %s/%s", address.ToString().CStr(), ov::StringFromSocketType(socket_type));
				return false;
			}
			else
			{
				logti("%s is listening on %s/%s", GetProviderName(), address.ToString().CStr(), ov::StringFromSocketType(socket_type));
			}

			physical_port->AddObserver(this);

			auto stream_map_item = std::make_shared<MpegTsStreamPortItem>(socket_type, port, physical_port);
			_stream_port_map.emplace(port, stream_map_item);
		}

		return true;
	}

	std::shared_ptr<MpegTsStreamPortItem> MpegTsProvider::GetDetachedStreamPortItem()
	{
		std::shared_lock<std::shared_mutex> lock(_stream_port_map_lock);

		for (const auto &item : _stream_port_map)
		{
			auto &stream_port_item = item.second;

			if (stream_port_item->IsAttached() == false)
			{
				return stream_port_item;
			}
		}

		return nullptr;
	}

	bool MpegTsProvider::Start()
	{
		auto &server_config = GetServerConfig();

		std::map<std::tuple<int, ov::SocketType>, StreamInfo> stream_map;

		if (server_config.GetBind().GetProviders().GetMpegts().IsParsed() == false)
		{
			logti("%s is disabled by configuration", GetProviderName());
			return true;
		}

		if (BindMpegTSPorts() == false)
		{
			return false;
		}

		StartTimer();

		return Provider::Start();
	}

	bool MpegTsProvider::Stop()
	{
		for (const auto &x : _stream_port_map)
		{
			auto stream_port_item = x.second;
			auto physical_port = stream_port_item->GetPhysicalPort();
			physical_port->RemoveObserver(this);
			PhysicalPortManager::GetInstance()->DeletePort(physical_port);
		}
		_stream_port_map.clear();

		StopTimer();

		return true;
	}

	std::shared_ptr<pvd::Application> MpegTsProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		if(IsModuleAvailable() == false)
		{
			return nullptr;
		}

		auto &app_config = application_info.GetConfig();
		auto &stream_list = app_config.GetProviders().GetMpegtsProvider().GetStreamMap().GetStreamList();
		auto app_metrics = ApplicationMetrics(application_info);

		for (auto &stream_item : stream_list)
		{
			bool is_parsed;
			auto &port_config = stream_item.GetPort(&is_parsed);
			std::vector<int> port_list;

			// If they want to use any available port
			if (is_parsed == false)
			{
				// Get random port
				auto stream_port_item = GetDetachedStreamPortItem();
				if (stream_port_item == nullptr)
				{
					logte("The %s application could not be created in %s provider because there are no ports available.",
						  application_info.GetName().CStr(), GetProviderName());
					return nullptr;
				}

				port_list.push_back(stream_port_item->GetPortNumber());
			}
			else
			{
				port_list = port_config.GetPortList();
			}

			for (auto port : port_list)
			{
				// Search for the port in bound mpegts ports map
				auto stream_port_item = GetStreamPortItem(port);
				if (stream_port_item == nullptr || stream_port_item->IsAttached() == true)
				{
					logte("The %s application could not be created in %s provider because port %d requested to be assigned to mpegts is not available.",
						  application_info.GetName().CStr(), GetProviderName(), port);
					return nullptr;
				}

				auto stream_name = stream_item.GetName().Replace("${Port}", ov::Converter::ToString(port));
				stream_port_item->AttachToApplication(application_info.GetName(), stream_name);

				auto url = ov::Url::Parse(ov::String::FormatString("%s://0.0.0.0:%d", ov::StringFromSocketType(stream_port_item->GetScheme()), stream_port_item->GetPortNumber()));
				app_metrics->OnStreamReserved(GetProviderType(), *url, stream_name);
			}
		}

		return MpegTsApplication::Create(GetSharedPtrAs<pvd::PushProvider>(), application_info);
	}

	bool MpegTsProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_port_map_lock);

		for (const auto &item : _stream_port_map)
		{
			auto &stream_port_item = item.second;
			if (stream_port_item->GetVhostAppName() == application->GetName())
			{
				stream_port_item->DetachFromApplication();
			}
		}

		return PushProvider::OnDeleteProviderApplication(application);
	}

	std::shared_ptr<MpegTsStreamPortItem> MpegTsProvider::GetStreamPortItem(uint16_t local_port)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_port_map_lock);

		auto x = _stream_port_map.find(local_port);
		if (x == _stream_port_map.end())
		{
			// something wrong
			return nullptr;
		}
		return x->second;
	}

	// This function is not called by PhysicalPort when the protocol is udp (MPEGTS/UDP)
	// It will be called by OnDataReceived when first packet is arrived from client
	void MpegTsProvider::OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		auto local_port = remote->GetLocalAddress()->Port();
		auto channel_id = remote->GetNativeHandle();

		auto stream_port_item = GetStreamPortItem(local_port);
		if (stream_port_item == nullptr || stream_port_item->IsAttached() == false)
		{
			return;
		}

		auto stream = MpegTsStream::Create(StreamSourceType::Mpegts, channel_id, stream_port_item->GetVhostAppName(), stream_port_item->GetOutputStreamName(), remote, GetSharedPtrAs<pvd::PushProvider>());
		if (PushProvider::OnChannelCreated(remote->GetNativeHandle(), stream) == true)
		{
			logti("A MPEG-TS client has connected");  // %s", remote->ToString().CStr());
			stream_port_item->OnClientConnected(channel_id);
			// If this stream does not send data for 3 seconds, it is determined that the connection has been lost.
			// Because the stream is over UDP
			SetChannelTimeout(stream, 3);
		}
	}

	void MpegTsProvider::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
										const ov::SocketAddress &address,
										const std::shared_ptr<const ov::Data> &data)
	{
		auto local_port = remote->GetLocalAddress()->Port();
		auto channel_id = remote->GetNativeHandle();

		auto stream_port_item = GetStreamPortItem(local_port);
		if (stream_port_item == nullptr)
		{
			logtc("Could not find StreamPortItem matching");  // %s", remote->ToString().CStr());
			return;
		}

		if (stream_port_item->IsClientConnected() == false)
		{
			OnConnected(remote);
		}

		PushProvider::OnDataReceived(channel_id, data);
	}

	void MpegTsProvider::OnTimer(const std::shared_ptr<PushStream> &channel)
	{
		auto mpegts_stream = std::dynamic_pointer_cast<MpegTsStream>(channel);

		logti("The client has not sent data for %d seconds. Stream has been deleted: [%s/%s], remote: ",  //%s",
			  mpegts_stream->GetElapsedSecSinceLastReceived(),
			  mpegts_stream->GetApplicationName(), mpegts_stream->GetName().CStr());
		//mpegts_stream->GetClientSock()->ToString().CStr());

		auto stream_port_item = GetStreamPortItem(mpegts_stream->GetClientSock()->GetLocalAddress()->Port());
		if (stream_port_item == nullptr)
		{
			logtc("Could not find StreamPortItem matching");  // %s", remote->ToString().CStr());
			return;
		}

		stream_port_item->OnClientDisconnected();

		PushProvider::OnChannelDeleted(channel);
	}

	// This function is not called by PhysicalPort when the protocol is udp (MPEGTS/UDP)
	void MpegTsProvider::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
										PhysicalPortDisconnectReason reason,
										const std::shared_ptr<const ov::Error> &error)
	{
		auto channel = GetChannel(remote->GetNativeHandle());
		if (channel == nullptr)
		{
			logte("Failed to find channel to delete stream (remote : ");  //%s)", remote->ToString().CStr());
			return;
		}

		logti("The MPEGTS client has disconnected: [%s/%s], remote: ",	//%s",
			  channel->GetApplicationName(), channel->GetName().CStr());
		//remote->ToString().CStr());

		PushProvider::OnChannelDeleted(remote->GetNativeHandle());
	}
}  // namespace pvd