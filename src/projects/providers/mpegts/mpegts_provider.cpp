//==============================================================================
//
//  MPEGTS Provider
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_provider.h"
#include "mpegts_application.h"
#include "mpegts_stream.h"
#include "mpegts_provider_private.h"

#include <modules/physical_port/physical_port_manager.h>

namespace pvd
{
	std::shared_ptr<MpegTsProvider> MpegTsProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<MpegTsProvider>(server_config, router);
		if (!provider->Start())
		{
			logte("An error occurred while creating MpegTS Provider");
			return nullptr;
		}
		return provider;
	}

	MpegTsProvider::MpegTsProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: PushProvider(server_config, router)
	{
		logtd("Created MpegTS Provider module.");
	}

	MpegTsProvider::~MpegTsProvider()
	{
		logti("Terminated MpegTS Provider module.");
	}

	bool MpegTsProvider::Start()
	{
		// TODO(Dimiden): It is temporary code
		// App/Stream/IP/Port/Protocol information has to be loaded from configuration
		for(int i=0; i<5; i++)
		{
			auto port = 40000 + i;
			ov::String app_name = ov::String::FormatString("#default#app");
			ov::String stream_name = ov::String::FormatString("stream_%d", port);
			auto protocol = ov::SocketType::Udp;

			auto address = ov::SocketAddress("*", port);

			auto physical_port = PhysicalPortManager::Instance()->CreatePort(protocol, address);
			if(physical_port == nullptr)
			{
				logte("Could not initialize phyiscal port for MPEG-TS server: %s", address.ToString().CStr());
				return false;
			}

			physical_port->AddObserver(this);

			auto stream_map = std::make_shared<MpegTsStreamPortItem>(port, app_name, stream_name, physical_port);

			_stream_port_map.emplace(port, stream_map);
		}

		StartTimer();

		return Provider::Start();
	}

	bool MpegTsProvider::Stop()
	{
		for(const auto &x : _stream_port_map)
		{
			auto stream_port_item = x.second;
			auto physical_port = stream_port_item->GetPhysicalPort();
			physical_port->RemoveObserver(this);
			PhysicalPortManager::Instance()->DeletePort(physical_port);
		}
		_stream_port_map.clear();

		StopTimer();

		return true;
	}

	std::shared_ptr<pvd::Application> MpegTsProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		return MpegTsApplication::Create(GetSharedPtrAs<pvd::PushProvider>(), application_info);
	}

	bool MpegTsProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return PushProvider::OnDeleteProviderApplication(application);
	}

	std::shared_ptr<MpegTsStreamPortItem> MpegTsProvider::GetStreamPortItem(uint16_t local_port)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_port_map_lock);

		auto x = _stream_port_map.find(local_port);
		if(x == _stream_port_map.end())
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
		auto channel_id = remote->GetId();

		auto stream_port_item = GetStreamPortItem(local_port);
		if(stream_port_item == nullptr)
		{
			return;
		}
		
		auto stream = MpegTsStream::Create(StreamSourceType::MpegTS, channel_id, stream_port_item->GetAppName(), stream_port_item->GetStreamName(), remote, GetSharedPtrAs<pvd::PushProvider>());	
		if(PushProvider::OnSignallingChannelCreated(remote->GetId(), stream) == true)
		{	
			logti("A MPEG-TS client has connected from");// %s", remote->ToString().CStr());
			stream_port_item->EnableClient(channel_id);
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
		auto channel_id = remote->GetId();

		auto stream_port_item = GetStreamPortItem(local_port);
		if(stream_port_item == nullptr)
		{
			logtc("Could not find StreamPortItem matching");// %s", remote->ToString().CStr());
			return;
		}

		if(stream_port_item->IsClientConnected() == false)
		{
			OnConnected(remote);
		}

		PushProvider::OnDataReceived(channel_id, data);
	}

	void MpegTsProvider::OnTimer(const std::shared_ptr<PushStream> &channel)
	{
		auto mpegts_stream = std::dynamic_pointer_cast<MpegTsStream>(channel);

		logti("The client has not sent data for %d seconds. Stream has been deleted: [%s/%s], remote: ", //%s",
					mpegts_stream->GetElapsedSecSinceLastReceived(),
					mpegts_stream->GetApplicationName(), mpegts_stream->GetName().CStr());
					//mpegts_stream->GetClientSock()->ToString().CStr());

		auto stream_port_item = GetStreamPortItem(mpegts_stream->GetClientSock()->GetLocalAddress()->Port());
		if(stream_port_item == nullptr)
		{
			logtc("Could not find StreamPortItem matching");// %s", remote->ToString().CStr());
			return;
		}

		stream_port_item->DisableClient();

		PushProvider::OnChannelDeleted(channel);
	}

	// This function is not called by PhysicalPort when the protocol is udp (MPEGTS/UDP)
	void MpegTsProvider::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
						PhysicalPortDisconnectReason reason,
						const std::shared_ptr<const ov::Error> &error)
	{
		auto channel = GetChannel(remote->GetId());
		if(channel == nullptr)
		{
			logte("Failed to find channel to delete stream (remote : ");//%s)", remote->ToString().CStr());
			return;
		}

		logti("The MPEGTS client has disconnected: [%s/%s], remote: ", //%s",
					channel->GetApplicationName(), channel->GetName().CStr());
					//remote->ToString().CStr());

		PushProvider::OnChannelDeleted(remote->GetId());
	}
}