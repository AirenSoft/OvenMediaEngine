//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_provider.h"

#include "webrtc_application.h"
#include "webrtc_private.h"
#include "webrtc_provider_signalling_interceptor.h"
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

	bool WebRTCProvider::StartSignallingServers(const cfg::Server &server_config, const cfg::bind::cmm::Webrtc &webrtc_bind_config)
	{
		auto &signalling_config = webrtc_bind_config.GetSignalling();

		bool is_configured;
		auto worker_count = signalling_config.GetWorkerCount(&is_configured);
		worker_count = is_configured ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

		bool is_port_configured;
		auto &port_config = signalling_config.GetPort(&is_port_configured);
		bool is_tls_port_configured;
		auto &tls_port_config = signalling_config.GetTlsPort(&is_tls_port_configured);

		if ((is_port_configured == false) && (is_tls_port_configured == false))
		{
			logtw("%s is disabled - No port is configured", GetProviderName());
			return true;
		}

		auto rtc_signalling_server = std::make_shared<RtcSignallingServer>(server_config, webrtc_bind_config);
		auto rtc_signalling_observer = RtcSignallingObserver::GetSharedPtr();

		auto whip_server = std::make_shared<WhipServer>(webrtc_bind_config);

		do
		{
			const auto &ip_list = server_config.GetIPList();

			// Initialize WebSocket Server
			rtc_signalling_server->AddObserver(rtc_signalling_observer);
			if (rtc_signalling_server->Start(
					GetProviderName(), "RtcSig",
					ip_list,
					is_port_configured, port_config.GetPort(),
					is_tls_port_configured, tls_port_config.GetPort(),
					worker_count, std::make_shared<WebRtcProviderSignallingInterceptor>()) == false)
			{
				break;
			}

			// Initialize WHIP Server
			if (whip_server->Start(
					WhipObserver::GetSharedPtr(),
					GetProviderName(), "WhpSig",
					ip_list,
					is_port_configured, port_config.GetPort(),
					is_tls_port_configured, tls_port_config.GetPort(),
					worker_count) == false)
			{
				break;
			}

			_signalling_server = std::move(rtc_signalling_server);
			_whip_server = std::move(whip_server);

			return true;
		} while (false);

		OV_SAFE_RESET(
			rtc_signalling_server, nullptr, {
				rtc_signalling_server->RemoveObserver(rtc_signalling_observer);
				rtc_signalling_server->Stop();
			},
			rtc_signalling_server);
		OV_SAFE_RESET(whip_server, nullptr, whip_server->Stop(), whip_server);

		return false;
	}

	bool WebRTCProvider::StartICEPorts(const cfg::Server &server_config, const cfg::bind::cmm::Webrtc &webrtc_bind_config)
	{
		auto ice_port = IcePortManager::GetInstance()->CreateTurnServers(
			GetProviderName(),
			IcePortObserver::GetSharedPtr(),
			server_config, webrtc_bind_config);

		if (ice_port == nullptr)
		{
			return false;
		}

		_ice_port = ice_port;

		_certificate = CreateCertificate();

		if (_certificate == nullptr)
		{
			logte("Could not create certificate.");
			return false;
		}

		return true;
	}

	bool WebRTCProvider::Start()
	{
		auto server_config = GetServerConfig();
		auto webrtc_bind_config = server_config.GetBind().GetProviders().GetWebrtc();

		if (webrtc_bind_config.IsParsed() == false)
		{
			logtw("%s is disabled by configuration", GetProviderName());
			return true;
		}

		if (StartSignallingServers(server_config, webrtc_bind_config) &&
			StartICEPorts(server_config, webrtc_bind_config))
		{
			return Provider::Start();
		}

		logte("An error occurred while initialize %s. Stopping RtcSignallingServer...", GetProviderName());

		IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

		return false;
	}

	bool WebRTCProvider::Stop()
	{
		IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

		if (_signalling_server != nullptr)
		{
			_signalling_server->RemoveObserver(RtcSignallingObserver::GetSharedPtr());
			_signalling_server->Stop();
		}

		if (_whip_server != nullptr)
		{
			_whip_server->Stop();
		}

		return Provider::Stop();
	}

	bool WebRTCProvider::OnCreateHost(const info::Host &host_info)
	{
		auto certificate = host_info.GetCertificate();

		if (certificate == nullptr)
		{
			return true;  // not failed, just no certificate
		}

		if (_signalling_server != nullptr)
		{
			_signalling_server->AppendCertificate(certificate);
		}

		if (_whip_server != nullptr)
		{
			_whip_server->AppendCertificate(certificate);
		}

		return true;
	}

	bool WebRTCProvider::OnDeleteHost(const info::Host &host_info)
	{
		auto certificate = host_info.GetCertificate();

		if (certificate == nullptr)
		{
			return true;  // not failed, just no certificate
		}

		if (_signalling_server != nullptr)
		{
			_signalling_server->RemoveCertificate(certificate);
		}

		if (_whip_server != nullptr)
		{
			_whip_server->RemoveCertificate(certificate);
		}

		return true;
	}

	std::shared_ptr<pvd::Application> WebRTCProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		if (IsModuleAvailable() == false)
		{
			return nullptr;
		}

		if (_whip_server != nullptr)
		{
			auto webrtc_cfg = application_info.GetConfig().GetProviders().GetWebrtcProvider();
			auto cross_domains = webrtc_cfg.GetCrossDomainList();
			if (cross_domains.empty())
			{
				// There is no CORS setting in the WebRTC Provider in the already deployed Server.xml. In this case, provide * to avoid confusion.
				cross_domains.push_back("*");
			}
			_whip_server->SetCors(application_info.GetName(), cross_domains);
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
		if (parsed_url->Port() == 0)
		{
			parsed_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
		}

		uint64_t session_life_time = 0;
		auto [signed_policy_result, signed_policy] = VerifyBySignedPolicy(parsed_url, remote_address);
		if (signed_policy_result == AccessController::VerificationResult::Off)
		{
			// Success
		}
		else if (signed_policy_result == AccessController::VerificationResult::Pass)
		{
			session_life_time = signed_policy->GetStreamExpireEpochMSec();
		}
		else if (signed_policy_result == AccessController::VerificationResult::Error)
		{
			return nullptr;
		}
		else if (signed_policy_result == AccessController::VerificationResult::Fail)
		{
			logtw("%s", signed_policy->GetErrMessage().CStr());
			return nullptr;
		}

		// Admission Webhooks
		auto request_info = std::make_shared<AccessController::RequestInfo>(parsed_url, remote_address, request->GetHeader("USER-AGENT"));

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
				if (session_life_time == 0 || stream_expired_msec_from_webhooks < session_life_time)
				{
					session_life_time = stream_expired_msec_from_webhooks;
				}
			}

			// Redirect URL
			if (admission_webhooks->GetNewURL() != nullptr)
			{
				parsed_url = admission_webhooks->GetNewURL();
				if (parsed_url->Port() == 0)
				{
					parsed_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
				}

				final_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(parsed_url->Host(), parsed_url->App());
				final_host_name = parsed_url->Host();
				final_stream_name = parsed_url->Stream();
			}
		}
		else if (webhooks_result == AccessController::VerificationResult::Error)
		{
			logtw("AdmissionWebhooks error : %s", parsed_url->ToUrlString().CStr());
			return nullptr;
		}
		else if (webhooks_result == AccessController::VerificationResult::Fail)
		{
			logtw("AdmissionWebhooks error : %s", admission_webhooks->GetErrReason().CStr());
			return nullptr;
		}

		// Check if same stream name is exist
		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(final_vhost_app_name));
		if (application == nullptr)
		{
			logte("Could not find %s application", final_vhost_app_name.CStr());
			return nullptr;
		}

		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if (application->GetStreamByName(final_stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return nullptr;
		}

		auto transport = parsed_url->GetQueryValue("transport");
		if (transport.UpperCaseString() == "TCP")
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
		ws_session->AddUserData("final_url", parsed_url->ToUrlString(true));
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
		if (autorized_exist == true && std::holds_alternative<bool>(authorized) == true && std::get<bool>(authorized) == true)
		{
			auto [final_url_exist, final_url] = ws_session->GetUserData("final_url");
			if (final_url_exist == true && std::holds_alternative<ov::String>(final_url) == true)
			{
				uri = std::get<ov::String>(final_url);
			}
			else
			{
				return false;
			}

			auto [stream_expired_exist, stream_expired] = ws_session->GetUserData("stream_expired");
			if (stream_expired_exist == true && std::holds_alternative<uint64_t>(stream_expired) == true)
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
		if (application == nullptr)
		{
			logte("Could not find %s application", final_vhost_app_name.CStr());
			return false;
		}

		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if (application->GetStreamByName(final_stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return false;
		}

		// Create Stream
		auto stream = WebRTCStream::Create(StreamSourceType::WebRTC, final_stream_name, PushProvider::GetSharedPtrAs<PushProvider>(), offer_sdp, peer_sdp, _certificate, _ice_port);
		if (stream == nullptr)
		{
			logte("Could not create %s stream in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return false;
		}
		stream->SetMediaSource(request->GetRemote()->GetRemoteAddressAsUrl());

		// The stream of the webrtc provider has already completed signaling at this point.
		if (PublishChannel(stream->GetId(), final_vhost_app_name, stream) == false)
		{
			return false;
		}

		if (OnChannelCreated(stream->GetId(), stream) == false)
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
		auto request = ws_session->GetRequest();
		auto parsed_url{ov::Url::Parse(request->GetUri())};
		auto remote_address{request->GetRemote()->GetRemoteAddress()};
		if (parsed_url && remote_address)
		{
			std::shared_ptr<ov::Url> new_url;

			auto [final_url_exist, final_url] = ws_session->GetUserData("final_url");
			if (final_url_exist == true && std::holds_alternative<ov::String>(final_url) == true)
			{
				ov::String uri = std::get<ov::String>(final_url);

				if (request->GetUri() != uri)
				{
					new_url = ov::Url::Parse(uri);
				}
			}

			auto request_info = std::make_shared<AccessController::RequestInfo>(parsed_url, remote_address, new_url, request->GetHeader("USER-AGENT"));

			SendCloseAdmissionWebhooks(request_info);
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
		catch (const std::bad_any_cast &e)
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
			case IcePortConnectionState::Closed: {
				logti("IcePort is disconnected. : (%s/%s) reason(%d)", stream->GetApplicationName(), stream->GetName().CStr(), state);

				// Signalling server will call OnStopCommand, then stream will be removed in that function
				_signalling_server->Disconnect(stream->GetApplicationInfo().GetName(), stream->GetName(), stream->GetPeerSDP());
				OnChannelDeleted(stream);

				break;
			}
			default:
				break;
		}
	}

	// WHIP API
	WhipObserver::Answer WebRTCProvider::OnSdpOffer(const std::shared_ptr<const http::svr::HttpRequest> &request,
													const std::shared_ptr<const SessionDescription> &offer_sdp)
	{
		auto uri = request->GetParsedUri();
		if (uri == nullptr || uri->Host().IsEmpty() || uri->App().IsEmpty() || uri->Stream().IsEmpty())
		{
			return {http::StatusCode::BadRequest, "Invalid URI"};
		}

		info::VHostAppName final_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(uri->Host(), uri->App());
		ov::String final_host_name = uri->Host();
		ov::String final_stream_name = uri->Stream();

		if (uri->Port() == 0)
		{
			uri->SetPort(request->GetRemote()->GetLocalAddress()->Port());
		}

		// Signed Policy & Admission Webhooks
		uint64_t session_life_time = 0;

		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(final_vhost_app_name));
		if (application == nullptr)
		{
			logte("Could not find %s application", final_vhost_app_name.CStr());
			return {http::StatusCode::NotFound, "Could not find application"};
		}

		std::lock_guard<std::mutex> lock(_stream_lock);

		if (application->GetStreamByName(final_stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return {http::StatusCode::Conflict, "Stream is already exist"};
		}

		std::vector<IceCandidate> ice_candidates;
		if (_ice_candidate_list.empty() == false)
		{
			auto candidate_index_to_send = _current_ice_candidate_index++ % _ice_candidate_list.size();
			const auto &candidates = _ice_candidate_list[candidate_index_to_send];

			ice_candidates.insert(ice_candidates.end(), candidates.cbegin(), candidates.cend());
		}

		auto answer_sdp = application->CreateAnswerSDP(offer_sdp, _ice_port->GenerateUfrag(), ice_candidates);
		if (answer_sdp == nullptr)
		{
			logte("Could not create answer SDP");
			return {http::StatusCode::InternalServerError, "Could not create answer SDP"};
		}

		auto stream = WebRTCStream::Create(StreamSourceType::WebRTC, final_stream_name, PushProvider::GetSharedPtrAs<PushProvider>(), answer_sdp, offer_sdp, _certificate, _ice_port);
		if (stream == nullptr)
		{
			logte("Could not create %s stream in %s application", final_stream_name.CStr(), final_vhost_app_name.CStr());
			return {http::StatusCode::InternalServerError, "Could not create stream"};
		}

		stream->SetMediaSource(request->GetRemote()->GetRemoteAddressAsUrl());

		if (PublishChannel(stream->GetId(), final_vhost_app_name, stream) == false)
		{
			return {http::StatusCode::InternalServerError, "Could not publish stream"};
		}

		if (OnChannelCreated(stream->GetId(), stream) == false)
		{
			return {http::StatusCode::InternalServerError, "Could not publish stream"};
		}

		auto ice_timeout = application->GetConfig().GetProviders().GetWebrtcProvider().GetTimeout();
		_ice_port->AddSession(IcePortObserver::GetSharedPtr(), stream->GetId(), answer_sdp, offer_sdp, ice_timeout, session_life_time, stream);

		return {stream->GetSessionKey(), ov::Random::GenerateString(8), answer_sdp, http::StatusCode::Created};
	}

	WhipObserver::Answer WebRTCProvider::OnTrickleCandidate(const std::shared_ptr<const http::svr::HttpRequest> &request,
															const ov::String &session_id,
															const ov::String &if_match,
															const std::shared_ptr<const SessionDescription> &patch)
	{
		return {http::StatusCode::NoContent, ""};
	}

	bool WebRTCProvider::OnSessionDelete(const std::shared_ptr<const http::svr::HttpRequest> &request, const ov::String &session_key)
	{
		auto uri = request->GetParsedUri();
		if (uri == nullptr || uri->Host().IsEmpty() || uri->App().IsEmpty() || uri->Stream().IsEmpty())
		{
			return false;
		}

		info::VHostAppName vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(uri->Host(), uri->App());
		ov::String stream_name = uri->Stream();

		// Find Stream
		auto stream = std::static_pointer_cast<WebRTCStream>(GetStreamByName(vhost_app_name, stream_name));
		if (!stream)
		{
			logte("To stop stream failed. Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
			return false;
		}

		if (stream->GetSessionKey() != session_key)
		{
			logte("To stop stream failed. Invalid session key : expected(%s) / actual(%s)", stream->GetSessionKey().CStr(), session_key.CStr());
			return false;
		}

		_ice_port->RemoveSession(stream->GetId());

		return OnChannelDeleted(stream);
	}

	void WebRTCProvider::OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data)
	{
		logtp("WebRTCProvider::OnDataReceived");

		std::shared_ptr<WebRTCStream> stream;
		try
		{
			stream = std::any_cast<std::shared_ptr<WebRTCStream>>(user_data);
		}
		catch (const std::bad_any_cast &e)
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
		if (error != nullptr)
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
}  // namespace pvd
