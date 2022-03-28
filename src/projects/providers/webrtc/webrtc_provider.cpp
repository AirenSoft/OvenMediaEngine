//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_private.h"
#include "webrtc_provider.h"
#include "webrtc_provider_signalling_interceptor.h"
#include "webrtc_application.h"
#include "webrtc_stream.h"

namespace pvd
{
	std::shared_ptr<WebRTCProvider> WebRTCProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto webrtc = std::make_shared<WebRTCProvider>(server_config, router);
		if (!webrtc->Start())
		{
			return nullptr;
		}
		return webrtc;
	}

	WebRTCProvider::WebRTCProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: pvd::PushProvider(server_config, router)
	{

	}

	WebRTCProvider::~WebRTCProvider()
	{
		logtd("WebRTCProvider has been terminated finally");
	}

	//------------------------
	// Provider
	//------------------------

	bool WebRTCProvider::Start()
	{
		auto server_config = GetServerConfig();
		auto webrtc_bind_config = server_config.GetBind().GetProviders().GetWebrtc();
		
		if (webrtc_bind_config.IsParsed() == false)
		{
			logti("%s is disabled by configuration", GetProviderName());
			return true;
		}

		auto &signalling_config = webrtc_bind_config.GetSignalling();
		auto &port_config = signalling_config.GetPort();
		auto &tls_port_config = signalling_config.GetTlsPort();
		bool is_parsed;

		auto worker_count = signalling_config.GetWorkerCount(&is_parsed);
		worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

		auto port = static_cast<uint16_t>(port_config.GetPort());
		auto tls_port = static_cast<uint16_t>(tls_port_config.GetPort());
		bool has_port = (port != 0);
		bool has_tls_port = (tls_port != 0);

		if ((has_port == false) && (has_tls_port == false))
		{
			logte("Invalid WebRTC Port settings");
			return false;
		}

		ov::SocketAddress signalling_address = ov::SocketAddress(server_config.GetIp(), port);
		ov::SocketAddress signalling_tls_address = ov::SocketAddress(server_config.GetIp(), tls_port);

		// Initialize RtcSignallingServer
		auto interceptor = std::make_shared<WebRtcProviderSignallingInterceptor>();
		_signalling_server = std::make_shared<RtcSignallingServer>(server_config, server_config.GetBind().GetProviders().GetWebrtc());
		_signalling_server->AddObserver(RtcSignallingObserver::GetSharedPtr());
		if (_signalling_server->Start(has_port ? &signalling_address : nullptr, has_tls_port ? &signalling_tls_address : nullptr, worker_count, interceptor) == false)
		{
			return false;
		}

		bool result = true;

		_ice_port = IcePortManager::GetInstance()->CreatePort(IcePortObserver::GetSharedPtr());
		if (_ice_port == nullptr)
		{
			logte("Could not initialize ICE Port. Check your ICE configuration");
			result = false;
		}

		auto &ice_candidates_config = webrtc_bind_config.GetIceCandidates();

		if(IcePortManager::GetInstance()->CreateIceCandidates(IcePortObserver::GetSharedPtr(), ice_candidates_config) == false)
		{
			logte("Could not create ICE Candidates. Check your ICE configuration");
			result = false;
		}
		
		bool tcp_relay_parsed = false;
		auto tcp_relay = ice_candidates_config.GetTcpRelay(&tcp_relay_parsed);
		if(tcp_relay_parsed)
		{
			auto items = tcp_relay.Split(":");
			if(items.size() != 2)
			{
				logte("TcpRelay format is incorrect : <Relay IP>:<Port>");
			}
			else
			{
				bool tcp_relay_bind_parsed { false };
				auto tcp_relay_bind { webrtc_bind_config.GetTcpRelayBind(&tcp_relay_bind_parsed) };
				if(tcp_relay_bind_parsed)
				{
					auto l_tokens { tcp_relay_bind.Split(":") };
					if(l_tokens.size() == 2)
					{
						auto l_ip { (l_tokens[0].IsEmpty()) ? server_config.GetIp() : l_tokens[0] };
						auto l_port { l_tokens[1] };
						tcp_relay_bind = ov::String::FormatString("%s:%s", l_ip.CStr(), l_port.CStr());
						ov::SocketAddress l_tcp_address(tcp_relay_bind);
						tcp_relay_bind_parsed = l_tcp_address.IsValid();
						if(!tcp_relay_bind_parsed)
						{
							logte("TcpRelayBind invalid address: %s", tcp_relay_bind.CStr());
						}
					}
					else
					{
						tcp_relay_bind_parsed = false;
						logte("TcpRelayBind format is incorrect: <Relay Local IP>:<Port>");
					}
				}

				auto tcp_relay_listen { (tcp_relay_bind_parsed) ? tcp_relay_bind : ov::String::FormatString("*:%s", items[1].CStr()) };
				ov::SocketAddress tcp_relay_address(tcp_relay_listen);

				bool tcp_relay_worker_count_parsed { false };
				auto tcp_relay_worker_count = ice_candidates_config.GetTcpRelayWorkerCount(&tcp_relay_worker_count_parsed);
				tcp_relay_worker_count = tcp_relay_worker_count_parsed ? tcp_relay_worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

				if(IcePortManager::GetInstance()->CreateTurnServer(IcePortObserver::GetSharedPtr(), tcp_relay_address, ov::SocketType::Tcp, tcp_relay_worker_count) == false)
				{
					logte("Could not create Turn Server. Check your configuration");
					result = false;
				}
			}
		}

		_certificate = CreateCertificate();
		if(_certificate == nullptr)
		{
			logte("Could not create certificate.");
			result = false;
		}

		if (result)
		{
			logti("%s is listening on %s%s%s%s...",
				GetProviderName(),
				has_port ? signalling_address.ToString().CStr() : "",
				(has_port && has_tls_port) ? ", " : "",
				has_tls_port ? "TLS: " : "",
				has_tls_port ? signalling_tls_address.ToString().CStr() : "");
		}
		else
		{
			// Rollback
			logte("An error occurred while initialize WebRTC Provider. Stopping RtcSignallingServer...");

			_signalling_server->Stop();
			IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

			return false;
		}

		return Provider::Start();
	}

	bool WebRTCProvider::Stop()
	{
		IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

		if(_signalling_server != nullptr)
		{
			_signalling_server->RemoveObserver(RtcSignallingObserver::GetSharedPtr());
			_signalling_server->Stop();
		}

		return Provider::Stop();
	}

	bool WebRTCProvider::OnCreateHost(const info::Host &host_info)
	{
		if(_signalling_server != nullptr && host_info.GetCertificate() != nullptr)
		{
			return _signalling_server->AppendCertificate(host_info.GetCertificate());
		}

		return true;
	}

	bool WebRTCProvider::OnDeleteHost(const info::Host &host_info)
	{
		if(_signalling_server != nullptr && host_info.GetCertificate() != nullptr)
		{
			return _signalling_server->RemoveCertificate(host_info.GetCertificate());
		}
		return true;
	}

	std::shared_ptr<pvd::Application> WebRTCProvider::OnCreateProviderApplication(const info::Application &application_info)
	{	
		if(IsModuleAvailable() == false)
		{
			return nullptr;
		}

		return WebRTCApplication::Create(PushProvider::GetSharedPtrAs<PushProvider>(), application_info, _certificate, _ice_port, _signalling_server);
	}

	bool WebRTCProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return true;
	}

	//------------------------
	// Signalling
	//------------------------

	std::shared_ptr<const SessionDescription> WebRTCProvider::OnRequestOffer(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
													const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
													std::vector<RtcIceCandidate> *ice_candidates, bool &tcp_relay)
	{
		info::VHostAppName final_vhost_app_name = vhost_app_name;
		ov::String final_host_name = host_name;
		ov::String final_stream_name = stream_name;

		logtd("WebRTCProvider::OnAddRemoteDescription");
		auto request = ws_session->GetRequest();
		auto remote_address = request->GetRemote()->GetRemoteAddress();
		auto uri = request->GetUri();
		auto parsed_url = ov::Url::Parse(uri);
		if (parsed_url == nullptr)
		{
			logte("Could not parse the url: %s", uri.CStr());
			return nullptr;
		}

		// PORT can be omitted if port is rtmp default port, but SignedPolicy requires this information.
		if(parsed_url->Port() == 0)
		{
			parsed_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
		}

		uint64_t session_life_time = 0;
		auto [signed_policy_result, signed_policy] = VerifyBySignedPolicy(parsed_url, remote_address);
		if(signed_policy_result == AccessController::VerificationResult::Off)
		{
			// Success
		}
		else if(signed_policy_result == AccessController::VerificationResult::Pass)
		{
			session_life_time = signed_policy->GetStreamExpireEpochMSec();
		}
		else if(signed_policy_result == AccessController::VerificationResult::Error)
		{
			return nullptr;
		}
		else if(signed_policy_result == AccessController::VerificationResult::Fail)
		{
			logtw("%s", signed_policy->GetErrMessage().CStr());
			return nullptr;
		}

		// Admission Webhooks
		auto [webhooks_result, admission_webhooks] = VerifyByAdmissionWebhooks(parsed_url, remote_address);
		if(webhooks_result == AccessController::VerificationResult::Off)
		{
			// Success
		}
		else if(webhooks_result == AccessController::VerificationResult::Pass)
		{
			// Lifetime
			if(admission_webhooks->GetLifetime() != 0)
			{
				// Choice smaller value
				auto stream_expired_msec_from_webhooks = ov::Clock::NowMSec() + admission_webhooks->GetLifetime();
				if(session_life_time == 0 || stream_expired_msec_from_webhooks < session_life_time)
				{
					session_life_time = stream_expired_msec_from_webhooks;
				}
			}

			// Redirect URL
			if(admission_webhooks->GetNewURL() != nullptr)
			{
				parsed_url = admission_webhooks->GetNewURL();
				if(parsed_url->Port() == 0)
				{
					parsed_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
				}

				final_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(parsed_url->Host(), parsed_url->App());
				final_host_name = parsed_url->Host();
				final_stream_name = parsed_url->Stream();
			}
		}
		else if(webhooks_result == AccessController::VerificationResult::Error)
		{
			logtw("AdmissionWebhooks error : %s", parsed_url->ToUrlString().CStr());
			return nullptr;
		}
		else if(webhooks_result == AccessController::VerificationResult::Fail)
		{
			logtw("AdmissionWebhooks error : %s", admission_webhooks->GetErrReason().CStr());
			return nullptr;
		}

		// Check if same stream name is exist
		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(final_vhost_app_name));
		if(application == nullptr)
		{
			logte("Could not find %s application", final_vhost_app_name.CStr());
			return nullptr;
		}
		
		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if(application->GetStreamByName(final_stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return nullptr;
		}

		auto transport = parsed_url->GetQueryValue("transport");
		if(transport.UpperCaseString() == "TCP")
		{
			tcp_relay = true;
		}

		if (_ice_candidate_list.empty() == false)
		{
			auto candidate_index_to_send = _current_ice_candidate_index++ % _ice_candidate_list.size();
			const auto &candidates = _ice_candidate_list[candidate_index_to_send];

			ice_candidates->insert(ice_candidates->end(), candidates.cbegin(), candidates.cend());
		}

		auto session_description = std::make_shared<SessionDescription>(*application->GetOfferSDP());
		session_description->SetOrigin("OvenMediaEngine", ov::Unique::GenerateUint32(), 2, "IN", 4, "127.0.0.1");
		session_description->SetIceUfrag(_ice_port->GenerateUfrag());
		session_description->Update();

		// Passed AccessControl
		ws_session->AddUserData("authorized", true);
		ws_session->AddUserData("new_url", parsed_url->ToUrlString(true));
		ws_session->AddUserData("stream_expired", session_life_time);

		return session_description;
	}

	bool WebRTCProvider::OnAddRemoteDescription(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
		auto [autorized_exist, authorized] = ws_session->GetUserData("authorized");
		ov::String uri;
		uint64_t session_life_time = 0;
		if(autorized_exist == true && std::holds_alternative<bool>(authorized) == true && std::get<bool>(authorized) == true)
		{
			auto [new_url_exist, new_url] = ws_session->GetUserData("new_url");
			if(new_url_exist == true && std::holds_alternative<ov::String>(new_url) == true)
			{
				uri = std::get<ov::String>(new_url);
			}
			else
			{
				return false;
			}

			auto [stream_expired_exist, stream_expired] = ws_session->GetUserData("stream_expired");
			if(stream_expired_exist == true && std::holds_alternative<uint64_t>(stream_expired) == true)
			{
				session_life_time = std::get<uint64_t>(stream_expired);
			}
			else
			{
				return false;
			}
		}
		else
		{
			// This client was unauthoized when request offer situation
			return false;
		}

		auto parsed_url = ov::Url::Parse(uri);
		if (parsed_url == nullptr)
		{
			logte("Could not parse the url: %s", uri.CStr());
			return false;
		}

		auto final_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(parsed_url->Host(), parsed_url->App());
		auto final_stream_name = parsed_url->Stream();

		logtd("WebRTCProvider::OnAddRemoteDescription");
		auto request = ws_session->GetRequest();
		auto remote_address = request->GetRemote()->GetRemoteAddress();
		
		// Check if same stream name is exist
		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(final_vhost_app_name));
		if(application == nullptr)
		{
			logte("Could not find %s application", final_vhost_app_name.CStr());
			return false;
		}
		
		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if(application->GetStreamByName(final_stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return false;
		}

		// Create Stream
		auto channel_id = offer_sdp->GetSessionId();
		auto stream = WebRTCStream::Create(StreamSourceType::WebRTC, final_stream_name, channel_id, PushProvider::GetSharedPtrAs<PushProvider>(), offer_sdp, peer_sdp, _certificate, _ice_port);
		if(stream == nullptr)
		{
			logte("Could not create %s stream in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return false;
		}
		stream->SetMediaSource(request->GetRemote()->GetRemoteAddressAsUrl());
		
		// The stream of the webrtc provider has already completed signaling at this point.
		if(PublishChannel(channel_id, final_vhost_app_name, stream) == false)
		{
			return false;
		}

		if(OnChannelCreated(channel_id, stream) == false)
		{
			return false;
		}

		auto ice_timeout = application->GetConfig().GetProviders().GetWebrtcProvider().GetTimeout();
		_ice_port->AddSession(IcePortObserver::GetSharedPtr(), stream->GetId(), offer_sdp, peer_sdp, ice_timeout, session_life_time, stream);

		return true;
	}

	bool WebRTCProvider::OnIceCandidate(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
						const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
						const std::shared_ptr<RtcIceCandidate> &candidate,
						const ov::String &username_fragment)
	{
		return true;
	}

	bool WebRTCProvider::OnStopCommand(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
					const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
					const std::shared_ptr<const SessionDescription> &offer_sdp,
					const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
		logti("Stop command received : %s/%s/%u", vhost_app_name.CStr(), stream_name.CStr(), offer_sdp->GetSessionId());

		// Send Close to Admission Webhooks
		auto parsed_url { ov::Url::Parse(ws_session->GetRequest()->GetUri()) };
		auto remote_address { ws_session->GetRequest()->GetRemote()->GetRemoteAddress() };
		if (parsed_url && remote_address)
		{
			SendCloseAdmissionWebhooks(parsed_url, remote_address);
		}
		// the return check is not necessary
		
		// Find Stream
		auto stream = std::static_pointer_cast<WebRTCStream>(GetStreamByName(vhost_app_name, stream_name));
		if (!stream)
		{
			logte("To stop stream failed. Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		_ice_port->RemoveSession(stream->GetId());

		return OnChannelDeleted(stream);
	}

	//------------------------
	// IcePort
	//------------------------

	void WebRTCProvider::OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data)
	{
		logtd("WebRTCProvider::OnStateChanged : %d", state);

		std::shared_ptr<Application> application;
		std::shared_ptr<WebRTCStream> stream;
		try
		{
			stream = std::any_cast<std::shared_ptr<WebRTCStream>>(user_data);	
			application = stream->GetApplication();
		}
		catch(const std::bad_any_cast& e)
		{
			// Internal Error
			logtc("WebRTCProvider::OnDataReceived - Could not convert user_data, internal error");
			return;
		}
		
		switch (state)
		{
			case IcePortConnectionState::New:
			case IcePortConnectionState::Checking:
			case IcePortConnectionState::Connected:
			case IcePortConnectionState::Completed:
				// Nothing to do
				break;
			case IcePortConnectionState::Failed:
			case IcePortConnectionState::Disconnected:
			case IcePortConnectionState::Closed:
			{
				logti("IcePort is disconnected. : (%s/%s) reason(%d)", stream->GetApplicationName(), stream->GetName().CStr(),  state);

				// Signalling server will call OnStopCommand, then stream will be removed in that function
				_signalling_server->Disconnect(stream->GetApplicationInfo().GetName(), stream->GetName(), stream->GetPeerSDP());
				OnChannelDeleted(stream);

				break;
			}
			default:
				break;
		}
	}

	void WebRTCProvider::OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data)
	{
		logtd("WebRTCProvider::OnDataReceived");

		std::shared_ptr<WebRTCStream> stream;
		try
		{
			stream = std::any_cast<std::shared_ptr<WebRTCStream>>(user_data);	
		}
		catch(const std::bad_any_cast& e)
		{
			// Internal Error
			logtc("WebRTCProvider::OnDataReceived - Could not convert user_data, internal error");
			return;
		}

		PushProvider::OnDataReceived(stream->GetId(), data);
	}

	std::shared_ptr<Certificate> WebRTCProvider::CreateCertificate()
	{
		auto certificate = std::make_shared<Certificate>();

		auto error = certificate->Generate();
		if(error != nullptr)
		{
			logte("Cannot create certificate: %s", error->What());
			return nullptr;
		}

		return certificate;
	}

	std::shared_ptr<Certificate> WebRTCProvider::GetCertificate()
	{
		return _certificate;
	}
}
