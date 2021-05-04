//==============================================================================
//
//  SRT Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "srt_provider.h"

#include <modules/physical_port/physical_port_manager.h>
#include <orchestrator/orchestrator.h>

#include "srt_provider_private.h"
#include "providers/mpegts/mpegts_application.h"
#include "providers/mpegts/mpegts_stream.h"

namespace pvd
{
	std::shared_ptr<SrtProvider> SrtProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto provider = std::make_shared<SrtProvider>(server_config, router);
		if (!provider->Start())
		{
			return nullptr;
		}
		return provider;
	}

	SrtProvider::SrtProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: PushProvider(server_config, router)
	{
		logtd("Created SRT Provider module.");
	}

	SrtProvider::~SrtProvider()
	{
		logti("Terminated SRT Provider module.");
	}

	bool SrtProvider::Start()
	{
		auto &server_config = GetServerConfig();
		auto &srt_config = server_config.GetBind().GetProviders().GetSrt();

		if(srt_config.IsParsed() == false)
		{
			logti("%s is disabled by configuration", GetProviderName());
			return true;
		}

		auto srt_address = ov::SocketAddress(server_config.GetIp(), static_cast<uint16_t>(srt_config.GetPort().GetPort()));
		bool is_parsed;
		auto worker_count = srt_config.GetWorkerCount(&is_parsed);
		worker_count = is_parsed ? worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

		_physical_port = PhysicalPortManager::GetInstance()->CreatePort("SRT", ov::SocketType::Srt, srt_address, worker_count);
		if (_physical_port == nullptr)
		{
			logte("Could not initialize phyiscal port for SRT server: %s", srt_address.ToString().CStr());
			return false;
		}
		else
		{
			logti("%s is listening on %s/%s", GetProviderName(), srt_address.ToString().CStr(), ov::StringFromSocketType(ov::SocketType::Srt));
		}

		_physical_port->AddObserver(this);

		return Provider::Start();
	}

	bool SrtProvider::Stop()
	{


		return true;
	}

	std::shared_ptr<pvd::Application> SrtProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		if(IsModuleAvailable() == false)
		{
			return nullptr;
		}

		return MpegTsApplication::Create(GetSharedPtrAs<pvd::PushProvider>(), application_info);
	}

	bool SrtProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return PushProvider::OnDeleteProviderApplication(application);
	}

	void SrtProvider::OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		logti("The SRT client has connected : %s [%s]", remote->ToString().CStr(), remote->GetStreamId().CStr());
		
		// Get app/stream name from streamid
		auto streamid = remote->GetStreamId();

		// streamid format is below
		// srt://host[:port]/app/stream?query=value
		auto url = ov::Url::Parse(streamid);
		if(url == nullptr)
		{
			logte("SRT's streamid must be a URL of the form srt://{host}[:port]/{app}/{stream}?{query}={value} : %s", streamid.CStr());
			remote->Close();
			return;
		}

		auto channel_id = remote->GetNativeHandle();
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(url->Host(), url->App());
		auto stream_name = url->Stream();
		
		auto stream = MpegTsStream::Create(StreamSourceType::Srt, channel_id, vhost_app_name, stream_name, remote, GetSharedPtrAs<pvd::PushProvider>());

		PushProvider::OnChannelCreated(remote->GetNativeHandle(), stream);
	}

	void SrtProvider::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
										const ov::SocketAddress &address,
										const std::shared_ptr<const ov::Data> &data)
	{
		auto channel_id = remote->GetNativeHandle();
		PushProvider::OnDataReceived(channel_id, data);
	}

	void SrtProvider::OnTimer(const std::shared_ptr<PushStream> &channel)
	{
		PushProvider::OnChannelDeleted(channel);
	}

	void SrtProvider::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
										PhysicalPortDisconnectReason reason,
										const std::shared_ptr<const ov::Error> &error)
	{
		logtd("SrtProvider::OnDisonnected : %s [%s]", remote->ToString().CStr(), remote->GetStreamId().CStr());

		auto channel = GetChannel(remote->GetNativeHandle());
		if (channel == nullptr)
		{
			logte("Failed to find channel to delete stream (remote : %s)", remote->ToString().CStr());
			return;
		}

		logti("The SRT client has disconnected: [%s/%s], remote: %s", channel->GetApplicationName(), channel->GetName().CStr(), remote->ToString().CStr());

		PushProvider::OnChannelDeleted(remote->GetNativeHandle());
	}
}  // namespace pvd