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
#include <modules/srt/srt.h>
#include <orchestrator/orchestrator.h>

#include "providers/mpegts/mpegts_application.h"
#include "providers/mpegts/mpegts_stream.h"
#include "srt_provider_private.h"

namespace pvd
{
	std::shared_ptr<SrtProvider> SrtProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
	{
		auto provider = std::make_shared<SrtProvider>(server_config, router);
		if (!provider->Start())
		{
			return nullptr;
		}
		return provider;
	}

	SrtProvider::SrtProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
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
		auto &srt_bind_config = server_config.GetBind().GetProviders().GetSrt();

		if (srt_bind_config.IsParsed() == false)
		{
			logtw("%s is disabled by configuration", GetProviderName());
			return true;
		}

		// Init StreamMap if exist
		std::unique_lock<std::shared_mutex> lock{_stream_map_mutex};
		auto stream_map = srt_bind_config.GetStreamMap();
		for (const auto &stream : stream_map.GetStreamList())
		{
			_stream_map.emplace(stream.GetPort(), std::make_shared<StreamMap>(stream.GetPort(), stream.GetUrl()));
		}
		lock.unlock();

		bool is_configured;
		auto worker_count = srt_bind_config.GetWorkerCount(&is_configured);
		worker_count = is_configured ? worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

		bool is_port_configured;
		auto &port_config = srt_bind_config.GetPort(&is_port_configured);

		if (is_port_configured == false)
		{
			logtw("%s is disabled - No port is configured", GetProviderName());
			return true;
		}

		std::vector<ov::SocketAddress> address_list;
		try
		{
			address_list = ov::SocketAddress::Create(server_config.GetIPList(), port_config.GetPortList());
		}
		catch (const ov::Error &e)
		{
			logte("Could not listen for %s Server: %s", GetProviderName(), e.What());
			return false;
		}

		bool result = true;
		std::vector<ov::String> address_string_list;
		std::vector<std::shared_ptr<PhysicalPort>> physical_port_list;

		auto physical_port_manager = PhysicalPortManager::GetInstance();

		for (const auto &address : address_list)
		{
			auto physical_port = physical_port_manager->CreatePort(
				"SRT", ov::SocketType::Srt, address, worker_count, 0, 0,
				[=](const std::shared_ptr<ov::Socket> &socket) -> std::shared_ptr<ov::Error> {
					return SrtOptionProcessor::SetOptions(socket, srt_bind_config.GetOptions());
				});

			if (physical_port == nullptr)
			{
				logte("Could not initialize phyiscal port for %s on %s", GetProviderName(), address.ToString().CStr());
				result = false;
				break;
			}

			address_string_list.emplace_back(address.ToString());

			physical_port->AddObserver(this);
			physical_port_list.push_back(physical_port);
		}

		if (result)
		{
			logti("%s is listening on %s/%s",
				  GetProviderName(),
				  ov::String::Join(address_string_list, ", ").CStr(),
				  ov::StringFromSocketType(ov::SocketType::Srt));

			{
				std::lock_guard lock_guard{_physical_port_list_mutex};
				_physical_port_list = std::move(physical_port_list);
			}

			return Provider::Start();
		}

		for (auto &physical_port : physical_port_list)
		{
			physical_port->RemoveObserver(this);
			physical_port_manager->DeletePort(physical_port);
		}

		return false;
	}

	bool SrtProvider::Stop()
	{
		_physical_port_list_mutex.lock();
		auto physical_port_list = std::move(_physical_port_list);
		_physical_port_list_mutex.unlock();

		auto physical_port_manager = PhysicalPortManager::GetInstance();

		for (auto &physical_port : physical_port_list)
		{
			physical_port->RemoveObserver(this);
			physical_port_manager->DeletePort(physical_port);
		}

		return Provider::Stop();
	}

	bool SrtProvider::OnCreateHost(const info::Host &host_info)
	{
		return true;
	}

	bool SrtProvider::OnDeleteHost(const info::Host &host_info)
	{
		return true;
	}

	std::shared_ptr<pvd::Application> SrtProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		if (IsModuleAvailable() == false)
		{
			return nullptr;
		}

		auto application = MpegTsApplication::Create(GetSharedPtrAs<pvd::PushProvider>(), application_info);
		if (application == nullptr)
		{
			return nullptr;
		}

		auto audio_map = application_info.GetConfig().GetProviders().GetSrtProvider().GetAudioMap();
		application->AddAudioMapItems(audio_map);

		return application;
	}

	bool SrtProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return PushProvider::OnDeleteProviderApplication(application);
	}

	std::shared_ptr<SrtProvider::StreamMap> SrtProvider::GetStreamMap(int port)
	{
		std::shared_lock<std::shared_mutex> lock{_stream_map_mutex};
		auto it = _stream_map.find(port);
		if (it == _stream_map.end())
		{
			return nullptr;
		}

		return it->second;
	}

	void SrtProvider::OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		logti("The SRT client has connected : %s [%d] [%s]", remote->ToString().CStr(), remote->GetNativeHandle(), remote->GetStreamId().CStr());

		// Get app/stream name from streamid
		auto streamid = remote->GetStreamId();
		ov::String decoded_url;

		if (streamid.IsEmpty())
		{
			auto stream_map = GetStreamMap(remote->GetLocalAddress()->Port());
			if (stream_map == nullptr)
			{
				logte("There is no stream information in the streamid and stream map. : %s", remote->ToString().CStr());
				remote->Close();
				return;
			}

			decoded_url = stream_map->_listen_url;
		}
		else
		{
			// SRT AccessControl style : #!::u=abc123,bmd_uuid=1234567890...
			if (streamid.GetLength() > 4 && streamid.HasPrefix("#!::"))
			{
				// Remove #!::
				streamid = streamid.Substring(4);

				auto key_values = streamid.Split(",");
				// Extract u=xxx (user name)
				for (const auto &key_value : key_values)
				{
					auto key_value_pair = key_value.Split("=");
					if (key_value_pair.size() == 2 && key_value_pair[0] == "u")
					{
						streamid = key_value_pair[1];
						break;
					}
				}
			}

			// urlencode(srt://host[:port]/app/stream?query=value)
			decoded_url = ov::Url::Decode(streamid);
		}

		auto final_url = ov::Url::Parse(decoded_url);
		if (final_url == nullptr)
		{
			logte("SRT's streamid streamid must be in the following format, percent encoded. srt://{host}[:port]/{app}/{stream}?{query}={value} : %s", streamid.CStr());
			remote->Close();
			return;
		}

		auto requested_url = final_url;

		auto channel_id = remote->GetNativeHandle();
		auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());
		auto stream_name = final_url->Stream();

		//TODO(Getroot): For security enhancement,
		// it should be checked whether the actual ip:port is the same as the ip:port of streamid (after dns resolve if it is domain).

		auto remote_address = remote->GetRemoteAddress();
		auto request_info = std::make_shared<ac::RequestInfo>(final_url, nullptr, remote, nullptr);

		// SingedPolicy
		uint64_t life_time = 0;
		
		auto [signed_policy_result, signed_policy] = VerifyBySignedPolicy(request_info);
		if (signed_policy_result == AccessController::VerificationResult::Off)
		{
			// Success
		}
		else if (signed_policy_result == AccessController::VerificationResult::Pass)
		{
			life_time = signed_policy->GetStreamExpireEpochMSec();
		}
		else if (signed_policy_result == AccessController::VerificationResult::Error)
		{
			// will not reach here
			remote->Close();
			return;
		}
		else if (signed_policy_result == AccessController::VerificationResult::Fail)
		{
			logtw("%s", signed_policy->GetErrMessage().CStr());
			remote->Close();
			return;
		}

		auto [webhooks_result, admission_webhooks] = VerifyByAdmissionWebhooks(request_info);
		if (webhooks_result == AccessController::VerificationResult::Off)
		{
			// Success
		}
		else if (webhooks_result == AccessController::VerificationResult::Pass)
		{
			// Lifetime
			if (admission_webhooks->GetLifetime() != 0)
			{
				// Choice smaller value
				auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + admission_webhooks->GetLifetime();
				if (life_time == 0 || stream_expired_msec_from_webhooks < life_time)
				{
					life_time = stream_expired_msec_from_webhooks;
				}
			}

			// Redirect URL
			if (admission_webhooks->GetNewURL() != nullptr)
			{
				final_url = admission_webhooks->GetNewURL();
				vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());
				stream_name = final_url->Stream();
			}
		}
		else if (webhooks_result == AccessController::VerificationResult::Error)
		{
			logtw("AdmissionWebhooks error : %s", final_url->ToUrlString().CStr());
			remote->Close();
			return;
		}
		else if (webhooks_result == AccessController::VerificationResult::Fail)
		{
			logtw("AdmissionWebhooks error : %s", admission_webhooks->GetErrReason().CStr());
			remote->Close();
			return;
		}

		// Check if application is exist
		if (GetApplicationByName(vhost_app_name) == nullptr)
		{
			logte("Could not find vhost/app: %s", vhost_app_name.CStr());
			remote->Close();
			return;
		}

		auto stream = MpegTsStream::Create(StreamSourceType::Srt, channel_id, vhost_app_name, stream_name, remote, *remote->GetRemoteAddress(), life_time, GetSharedPtrAs<pvd::PushProvider>());
		stream->SetRequestedUrl(requested_url);
		stream->SetFinalUrl(final_url);

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
		logti("SrtProvider::OnDisonnected : %s [%s]", remote->ToString().CStr(), remote->GetStreamId().CStr());

		auto channel = GetChannel(remote->GetNativeHandle());
		if (channel == nullptr)
		{
			// It probably rejected on OnConnected
			logtd("Failed to find channel to delete stream (remote : %s)", remote->ToString().CStr());
			return;
		}

		// Send Close to Admission Webhooks
		auto requested_url = channel->GetRequestedUrl();
		auto final_url = channel->GetFinalUrl();
		if (remote && requested_url && final_url)
		{
			auto request_info = std::make_shared<ac::RequestInfo>(requested_url, final_url, remote, nullptr);

			SendCloseAdmissionWebhooks(request_info);
		}
		// the return check is not necessary

		logti("The SRT client has disconnected: [%s/%s], remote: %s", channel->GetApplicationName(), channel->GetName().CStr(), remote->ToString().CStr());

		PushProvider::OnChannelDeleted(remote->GetNativeHandle());
	}
}  // namespace pvd
