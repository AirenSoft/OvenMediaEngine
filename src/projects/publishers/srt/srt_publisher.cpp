//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srt_publisher.h"

#include <base/ovlibrary/url.h>
#include <modules/srt/srt.h>

#include "srt_private.h"
#include "srt_session.h"

namespace pub
{
	std::shared_ptr<SrtPublisher> SrtPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
	{
		auto obj = std::make_shared<SrtPublisher>(server_config, router);

		return obj->Start() ? obj : nullptr;
	}

	SrtPublisher::SrtPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouterInterface> &router)
		: Publisher(server_config, router)
	{
	}

	SrtPublisher::~SrtPublisher()
	{
		logtd("%s has finally been terminated", GetPublisherName());
	}

	bool SrtPublisher::Start()
	{
		const auto &server_config = GetServerConfig();
		const auto &srt_bind_config = server_config.GetBind().GetPublishers().GetSrt();

		if (srt_bind_config.IsParsed() == false)
		{
			logtw("%s is disabled by configuration", GetPublisherName());
			return true;
		}

		// Init StreamMap if exist
		{
			std::unique_lock lock(_stream_map_mutex);

			auto stream_map = srt_bind_config.GetStreamMap();
			for (const auto &stream : stream_map.GetStreamList())
			{
				_stream_map.emplace(stream.GetPort(), std::make_shared<StreamMap>(stream.GetPort(), stream.GetStreamPath()));
			}
		}

		bool is_configured;
		auto &port_config = srt_bind_config.GetPort(&is_configured);

		if (is_configured == false)
		{
			logtw("SRT Publisher is disabled - No port is configured");
			return true;
		}

		auto &ip_list = server_config.GetIPList();
		std::vector<ov::SocketAddress> address_list;
		try
		{
			address_list = ov::SocketAddress::Create(ip_list, port_config.GetPortList());
		}
		catch (const ov::Error &e)
		{
			logte("Could not listen for %s Server: %s", GetPublisherName(), e.What());
			return false;
		}

		bool result = true;
		std::vector<ov::String> address_string_list;
		std::vector<std::shared_ptr<PhysicalPort>> physical_port_list;

		auto physical_port_manager = PhysicalPortManager::GetInstance();

		auto worker_count = srt_bind_config.GetWorkerCount(&is_configured);
		worker_count = is_configured ? worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

		for (const auto &address : address_list)
		{
			auto physical_port = physical_port_manager->CreatePort(
				"SRTPub", ov::SocketType::Srt, address, worker_count, 0, 0,
				[=](const std::shared_ptr<ov::Socket> &socket) -> std::shared_ptr<ov::Error> {
					return SrtOptionProcessor::SetOptions(socket, srt_bind_config.GetOptions());
				});

			if (physical_port == nullptr)
			{
				logte("Could not initialize phyiscal port for %s on %s", GetPublisherName(), address.ToString().CStr());
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
				  GetPublisherName(),
				  ov::String::Join(address_string_list, ", ").CStr(),
				  ov::StringFromSocketType(ov::SocketType::Srt));

			{
				std::lock_guard lock_guard(_physical_port_list_mutex);
				_physical_port_list = std::move(physical_port_list);
			}

			return Publisher::Start();
		}

		for (auto &physical_port : physical_port_list)
		{
			physical_port->RemoveObserver(this);
			physical_port_manager->DeletePort(physical_port);
		}

		return false;
	}

	bool SrtPublisher::Stop()
	{
		decltype(_physical_port_list) physical_port_list;
		{
			std::lock_guard lock_guard(_physical_port_list_mutex);
			physical_port_list = std::move(_physical_port_list);
		}

		for (auto &server_port : physical_port_list)
		{
			server_port->RemoveObserver(this);
			server_port->Close();
		}

		return Publisher::Stop();
	}

	bool SrtPublisher::OnCreateHost(const info::Host &host_info)
	{
		return true;
	}

	bool SrtPublisher::OnDeleteHost(const info::Host &host_info)
	{
		return true;
	}

	std::shared_ptr<Application> SrtPublisher::OnCreatePublisherApplication(const info::Application &application_info)
	{
		if (IsModuleAvailable() == false)
		{
			return nullptr;
		}

		return SrtApplication::Create(SrtPublisher::GetSharedPtrAs<Publisher>(), application_info);
	}

	bool SrtPublisher::OnDeletePublisherApplication(const std::shared_ptr<Application> &application)
	{
		return true;
	}

	std::shared_ptr<ov::Url> SrtPublisher::GetStreamUrlForRemote(const std::shared_ptr<ov::Socket> &remote, bool *is_vhost_form)
	{
		// stream_id can be in the following format:
		//
		//   #!::u=abc123,bmd_uuid=1234567890...
		//
		// https://github.com/Haivision/srt/blob/master/docs/features/access-control.md
		constexpr static const char SRT_STREAM_ID_PREFIX[] = "#!::";
		constexpr static const char SRT_USER_NAME_PREFIX[] = "u=";

		std::shared_ptr<ov::Url> parsed_url = nullptr;

		// Get app/stream name from stream_id
		auto streamid = remote->GetStreamId();

		bool is_vhost;

		// stream_path takes one of two forms:
		//
		// 1. urlencode(srt://host[:port]/app/stream[?query=value]) (deprecated)
		// 2. <vhost>/<app>/<stream>[?query=value]
		ov::String stream_path;

		if (streamid.IsEmpty())
		{
			auto stream_map = GetStreamMap(remote->GetLocalAddress()->Port());

			if (stream_map == nullptr)
			{
				logte("There is no stream information in the stream map for the SRT client: %s", remote->ToString().CStr());
				return nullptr;
			}

			stream_path = stream_map->stream_path;
		}
		else
		{
			auto final_streamid = streamid;
			ov::String user_name;
			bool from_user_name = false;

			if (final_streamid.HasPrefix(SRT_STREAM_ID_PREFIX))
			{
				// Remove the prefix `#!::`
				final_streamid = final_streamid.Substring(OV_COUNTOF(SRT_STREAM_ID_PREFIX) - 1);

				auto key_values = final_streamid.Split(",");

				// Extract user name part (u=xxx)
				for (const auto &key_value : key_values)
				{
					if (key_value.HasPrefix(SRT_USER_NAME_PREFIX))
					{
						final_streamid = key_value.Substring(OV_COUNTOF(SRT_USER_NAME_PREFIX) - 1);
						from_user_name = true;

						break;
					}
				}
			}

			if (final_streamid.IsEmpty())
			{
				if (from_user_name)
				{
					logte("Empty user name is not allowed in the streamid: [%s]", streamid.CStr());
				}
				else
				{
					logte("Empty streamid is not allowed");
				}

				return nullptr;
			}

			stream_path = ov::Url::Decode(final_streamid);

			if (stream_path.IsEmpty())
			{
				if (from_user_name)
				{
					logte("Invalid user name in the streamid: [%s] (streamid: [%s])", user_name.CStr(), streamid.CStr());
				}
				else
				{
					logte("Invalid streamid: [%s]", streamid.CStr());
				}

				return nullptr;
			}
		}

		if (stream_path.HasPrefix("srt://"))
		{
			// Deprecated format
			logtw("The srt://... format is deprecated. Use {vhost}/{app}/{stream} format instead: %s", streamid.CStr());
			is_vhost = false;
		}
		else
		{
			// {vhost}/{app}/{stream} format
			auto parts = stream_path.Split("/");

			if (parts.size() != 3)
			{
				logte("The streamid for SRT must be in the following format: {vhost}/{app}/{stream}, but [%s]", stream_path.CStr());
				return nullptr;
			}

			// Convert to srt://{vhost}/{app}/{stream}
			stream_path.Prepend("srt://");

			is_vhost = true;
		}

		auto final_url = stream_path.IsEmpty() ? nullptr : ov::Url::Parse(stream_path);

		if (final_url != nullptr)
		{
			if (is_vhost_form != nullptr)
			{
				*is_vhost_form = is_vhost;
			}
		}
		else
		{
			if (streamid.IsEmpty())
			{
				logte("The streamid for SRT must be in one of the following formats: srt://{host}[:{port}]/{app}/{stream}[?{query}={value}] or {vhost}/{app}/{stream}, but [%s]", stream_path.CStr());
			}
			else
			{
				logte("The streamid for SRT must be in one of the following formats: srt://{host}[:{port}]/{app}/{stream}[?{query}={value}] or {vhost}/{app}/{stream}, but [%s] (streamid: [%s])", stream_path.CStr(), streamid.CStr());
			}
		}

		return final_url;
	}

	void SrtPublisher::OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		auto orchestrator = ocst::Orchestrator::GetInstance();

		logti("The SRT client has connected: %s", remote->ToString().CStr());

		bool is_vhost_form;
		auto final_url = GetStreamUrlForRemote(remote, &is_vhost_form);

		if (final_url == nullptr)
		{
			remote->Close();
			return;
		}

		auto requested_url = final_url;

		auto remote_address = remote->GetRemoteAddress();
		auto request_info = std::make_shared<ac::RequestInfo>(final_url, nullptr, remote, nullptr);

		//
		// Handle SignedPolicy
		//
		uint64_t life_time = 0;
		auto [signed_policy_result, signed_policy] = VerifyBySignedPolicy(request_info);

		switch (signed_policy_result)
		{
			case AccessController::VerificationResult::Error:
				// will not reach here
				remote->Close();
				return;

			case AccessController::VerificationResult::Fail:
				logtw("%s", signed_policy->GetErrMessage().CStr());
				remote->Close();
				return;

			case AccessController::VerificationResult::Off:
				// Success
				break;

			case AccessController::VerificationResult::Pass:
				life_time = signed_policy->GetStreamExpireEpochMSec();
				break;
		}

		//
		// Handle AdmissionWebhooks
		//
		auto vhost_app_name = is_vhost_form
								  ? orchestrator->ResolveApplicationName(final_url->Host(), final_url->App())
								  : orchestrator->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());
		auto stream_name = final_url->Stream();

		auto [webhooks_result, admission_webhooks] = VerifyByAdmissionWebhooks(request_info);
		switch (webhooks_result)
		{
			case AccessController::VerificationResult::Error:
				logte("An error occurred while verifying with the AdmissionWebhooks: %s", final_url->ToUrlString().CStr());
				remote->Close();
				return;

			case AccessController::VerificationResult::Fail:
				logtw("AdmissionWebhooks server returns an error: %s", admission_webhooks->GetErrReason().CStr());
				remote->Close();
				return;

			case AccessController::VerificationResult::Off:
				// Success
				break;

			case AccessController::VerificationResult::Pass:
				// Lifetime
				if (admission_webhooks->GetLifetime() != 0)
				{
					// Choice smaller value
					auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + admission_webhooks->GetLifetime();
					if ((life_time == 0) || (stream_expired_msec_from_webhooks < life_time))
					{
						life_time = stream_expired_msec_from_webhooks;
					}
				}

				// Redirect URL
				if (admission_webhooks->GetNewURL() != nullptr)
				{
					final_url = admission_webhooks->GetNewURL();
					vhost_app_name = orchestrator->ResolveApplicationNameFromDomain(final_url->Host(), final_url->App());
					stream_name = final_url->Stream();
				}
				break;
		}

		// Check if application is exist
		auto application = GetApplicationByName(vhost_app_name);
		if (application == nullptr)
		{
			logte("Could not find vhost/app: %s", vhost_app_name.CStr());
			remote->Close();
			return;
		}

		auto stream = application->GetStream(final_url->Stream());

		if (stream == nullptr)
		{
			logte("Could not find stream: %s", final_url->Stream().CStr());

			::srt_setrejectreason(remote->GetNativeHandle(), 1404);
			remote->Close();
			return;
		}

		auto session = SrtSession::Create(application, stream, remote->GetNativeHandle(), remote);

		{
			std::unique_lock lock(_session_map_mutex);
			_session_map.emplace(session->GetId(), session);
		}

		session->SetRequestedUrl(requested_url);
		session->SetFinalUrl(final_url);

		stream->AddSession(session);
	}

	void SrtPublisher::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
									  const ov::SocketAddress &address,
									  const std::shared_ptr<const ov::Data> &data)
	{
		// Nothing to do
	}

	std::shared_ptr<SrtSession> SrtPublisher::GetSession(const std::shared_ptr<ov::Socket> &remote)
	{
		const auto session_id = remote->GetNativeHandle();

		std::shared_lock lock(_session_map_mutex);
		auto item = _session_map.find(session_id);

		return (item != _session_map.end()) ? item->second : nullptr;
	}

	void SrtPublisher::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
									  PhysicalPortDisconnectReason reason,
									  const std::shared_ptr<const ov::Error> &error)
	{
		auto session = GetSession(remote);

		if (session == nullptr)
		{
			logtd("Failed to find channel to delete SRT stream (remote : %s)", remote->ToString().CStr());
			return;
		}

		{
			std::unique_lock lock(_session_map_mutex);
			_session_map.erase(remote->GetNativeHandle());
		}

		auto stream = session->GetStream();
		stream->RemoveSession(session->GetId());

		// Send Close to Admission Webhooks
		auto requested_url = session->GetRequestedUrl();
		auto final_url = session->GetFinalUrl();

		if ((requested_url != nullptr) && (final_url != nullptr))
		{
			auto request_info = std::make_shared<ac::RequestInfo>(requested_url, final_url, remote, nullptr);

			// Checking the return value is not necessary
			SendCloseAdmissionWebhooks(request_info);
		}

		logti("The SRT client has disconnected: [%s/%s], %s", stream->GetApplicationName(), stream->GetName().CStr(), remote->ToString().CStr());
	}

	std::shared_ptr<SrtPublisher::StreamMap> SrtPublisher::GetStreamMap(int port)
	{
		std::shared_lock lock(_stream_map_mutex);

		auto item = _stream_map.find(port);

		return (item == _stream_map.end()) ? nullptr : item->second;
	}
}  // namespace pub
