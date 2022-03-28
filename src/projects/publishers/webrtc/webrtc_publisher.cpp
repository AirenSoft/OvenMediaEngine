#include <utility>

#include "rtc_private.h"
#include "rtc_session.h"
#include "rtc_stream.h"
#include "webrtc_publisher.h"
#include "webrtc_publisher_signalling_interceptor.h"
#include "config/config_manager.h"

#include <orchestrator/orchestrator.h>

std::shared_ptr<WebRtcPublisher> WebRtcPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto webrtc = std::make_shared<WebRtcPublisher>(server_config, router);

	if (!webrtc->Start())
	{
		return nullptr;
	}
	return webrtc;
}

WebRtcPublisher::WebRtcPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
}

WebRtcPublisher::~WebRtcPublisher()
{
	logtd("WebRtcPublisher has been terminated finally");
}

/*
 * Publisher Implementation
 */

bool WebRtcPublisher::Start()
{
	auto server_config = GetServerConfig();
	auto webrtc_bind_config = server_config.GetBind().GetPublishers().GetWebrtc();

	if (webrtc_bind_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto &signalling_config = webrtc_bind_config.GetSignalling();
	auto &port_config = signalling_config.GetPort();
	auto &tls_port_config = signalling_config.GetTlsPort();

	bool is_parsed;
	auto worker_count = signalling_config.GetWorkerCount(&is_parsed);
	worker_count = is_parsed ? worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

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
	auto interceptor = std::make_shared<WebRtcPublisherSignallingInterceptor>();
	_signalling_server = std::make_shared<RtcSignallingServer>(server_config, server_config.GetBind().GetPublishers().GetWebrtc());
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
			ov::String tcp_relay_bind { webrtc_bind_config.GetTcpRelayBind(&tcp_relay_bind_parsed) };
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

	if (result)
	{
		logti("%s is listening on %s%s%s%s...",
			  GetPublisherName(),
			  has_port ? signalling_address.ToString().CStr() : "",
			  (has_port && has_tls_port) ? ", " : "",
			  has_tls_port ? "TLS: " : "",
			  has_tls_port ? signalling_tls_address.ToString().CStr() : "");
	}
	else
	{
		// Rollback
		logte("An error occurred while initialize WebRTC Publisher. Stopping RtcSignallingServer...");

		_signalling_server->Stop();
		IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

		return false;
	}

	_message_thread.Start(ov::MessageThreadObserver<std::shared_ptr<ov::CommonMessage>>::GetSharedPtr());

	// Deprecated by Getroot 210830
	// _timer.Push(
	// 	[this](void *parameter) -> ov::DelayQueueAction 
	// 	{
	// 		// 2018-12-24 23:06:25.035,RTSP.SS,CONN_COUNT,INFO,,,[Live users], [Playback users]
	// 		std::shared_ptr<info::Application> rtsp_live_app_info;
	// 		std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
	// 		std::shared_ptr<info::Application> rtsp_play_app_info;
	// 		std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

	// 		rtsp_live_app_metrics = nullptr;
	// 		rtsp_play_app_metrics = nullptr;
			
	// 		// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications 
	// 		rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
	// 		if (rtsp_live_app_info != nullptr)
	// 		{
	// 			rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
	// 		}
	// 		rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
	// 		if (rtsp_play_app_info != nullptr)
	// 		{
	// 			rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
	// 		}

	// 		stat_log(STAT_LOG_WEBRTC_EDGE_VIEWERS, "%s,%s,%s,%s,,,%u,%u",
	// 				ov::Clock::Now().CStr(),
	// 				"WEBRTC.SS",
	// 				"CONN_COUNT",
	// 				"INFO",
	// 				rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
	// 				rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0);

	// 		return ov::DelayQueueAction::Repeat;
	// 	}
	// 	, 1000);

	// _timer.Start();

	return Publisher::Start();
}

bool WebRtcPublisher::Stop()
{
	IcePortManager::GetInstance()->Release(IcePortObserver::GetSharedPtr());

	if (_signalling_server)
	{
		_signalling_server->RemoveObserver(RtcSignallingObserver::GetSharedPtr());
		_signalling_server->Stop();
	}

	_message_thread.Stop();

	return Publisher::Stop();
}

bool WebRtcPublisher::DisconnectSession(const std::shared_ptr<RtcSession> &session)
{
	auto message = std::make_shared<ov::CommonMessage>();
	message->_code = static_cast<uint32_t>(MessageCode::DISCONNECT_SESSION);
	message->_message = std::make_any<std::shared_ptr<RtcSession>>(session);

	_message_thread.PostMessage(message);

	return true;
}

bool WebRtcPublisher::DisconnectSessionInternal(const std::shared_ptr<RtcSession> &session)
{
	auto stream = std::dynamic_pointer_cast<RtcStream>(session->GetStream());

	stream->RemoveSession(session->GetId());

	MonitorInstance->OnSessionDisconnected(*stream, PublisherType::Webrtc);

	session->Stop();

	// Special purpose log
	stat_log(STAT_LOG_WEBRTC_EDGE_SESSION, "%s,%s,%s,%s,,,%s,%s,%u",
					ov::Clock::Now().CStr(),
					"WEBRTC.SS",
					"SESSION",
					"INFO",
					"deleteClientSession",
					stream->GetName().CStr(),
					session->GetId());

	std::shared_ptr<info::Application> rtsp_live_app_info;
	std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
	std::shared_ptr<info::Application> rtsp_play_app_info;
	std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

	rtsp_live_app_metrics = nullptr;
	rtsp_play_app_metrics = nullptr;

	// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications 
	rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
	if (rtsp_live_app_info != nullptr)
	{
		rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
	}
	rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
	if (rtsp_play_app_info != nullptr)
	{
		rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
	}

	stat_log(STAT_LOG_WEBRTC_EDGE_SESSION, "%s,%s,%s,%s,,,%s:%d,%s:%d,%s,%u",
				ov::Clock::Now().CStr(),
				"WEBRTC.SS",
				"SESSION",
				"INFO",
				"Live",
				rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
				"Playback",
				rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0,
				stream->GetName().CStr(),
				session->GetId());

	return true;
}

void WebRtcPublisher::OnMessage(const std::shared_ptr<ov::CommonMessage> &message)
{
	auto code = static_cast<MessageCode>(message->_code);
	if(code == MessageCode::DISCONNECT_SESSION)
	{
		try 
		{
			auto session = std::any_cast<std::shared_ptr<RtcSession>>(message->_message);
			if(session == nullptr)
			{
				return;
			}

			_ice_port->RemoveSession(session->GetId());
			DisconnectSessionInternal(session);
		}
		catch(const std::bad_any_cast& e) 
		{
			logtc("Wrong message!");
			return;
		}
	}
}

bool WebRtcPublisher::OnCreateHost(const info::Host &host_info)
{
	if(_signalling_server != nullptr && host_info.GetCertificate() != nullptr)
	{
		return _signalling_server->AppendCertificate(host_info.GetCertificate());
	}

	return true;
}

bool WebRtcPublisher::OnDeleteHost(const info::Host &host_info)
{
	if(_signalling_server != nullptr && host_info.GetCertificate() != nullptr)
	{
		return _signalling_server->RemoveCertificate(host_info.GetCertificate());
	}
	return true;
}

std::shared_ptr<pub::Application> WebRtcPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if(IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return RtcApplication::Create(pub::Publisher::GetSharedPtrAs<pub::Publisher>(), application_info, _ice_port, _signalling_server);
}

bool WebRtcPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}

/*
 * Signalling Implementation
 */

// Called when receives request offer sdp from client
std::shared_ptr<const SessionDescription> WebRtcPublisher::OnRequestOffer(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
																		  const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
																		  std::vector<RtcIceCandidate> *ice_candidates, bool &tcp_relay)
{
	info::VHostAppName final_vhost_app_name = vhost_app_name;
	ov::String final_host_name = host_name;
	ov::String final_stream_name = stream_name;

	[[maybe_unused]] RequestStreamResult result = RequestStreamResult::init;
	auto request = ws_session->GetRequest();
	auto remote_address = request->GetRemote()->GetRemoteAddress();
	auto uri = request->GetUri();
	auto parsed_url = ov::Url::Parse(uri);
	if (parsed_url == nullptr)
	{
		logte("Could not parse the url: %s", uri.CStr());
		return nullptr;
	}

	// PORT can be omitted if port is default port, but SignedPolicy requires this information.
	if(parsed_url->Port() == 0)
	{
		parsed_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
	}

	uint64_t session_life_time = 0;
	std::shared_ptr<const SignedToken> signed_token;
	auto [signed_policy_result, signed_policy] =  Publisher::VerifyBySignedPolicy(parsed_url, remote_address);
	if(signed_policy_result == AccessController::VerificationResult::Pass)
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
	else if(signed_policy_result == AccessController::VerificationResult::Off)
	{
		// SingedToken
		auto [signed_token_result, signed_token] = Publisher::VerifyBySignedToken(parsed_url, remote_address);
		if(signed_token_result == AccessController::VerificationResult::Error)
		{
			return nullptr;
		}
		else if(signed_token_result == AccessController::VerificationResult::Fail)
		{
			logtw("%s", signed_token->GetErrMessage().CStr());
			return nullptr;
		}
		else if(signed_token_result == AccessController::VerificationResult::Pass)
		{
			session_life_time = signed_token->GetStreamExpiredTime();
		}
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

	auto stream = std::static_pointer_cast<RtcStream>(GetStream(final_vhost_app_name, final_stream_name));
	if(stream == nullptr)
	{
		stream = std::dynamic_pointer_cast<RtcStream>(PullStream(parsed_url, final_vhost_app_name, final_host_name, final_stream_name));
		if(stream == nullptr)
		{
			result = RequestStreamResult::origin_failed;
		}
		else
		{
			// Connection Request log
			// 2019-11-06 09:46:45.390 , RTSP.SS ,REQUEST,INFO,,,Live,rtsp://50.1.111.154:10915/1135/1/,220.103.225.254_44757_1573001205_389304_128855562
			stat_log(STAT_LOG_WEBRTC_EDGE_REQUEST, "%s,%s,%s,%s,,,%s,%s,%s",
						ov::Clock::Now().CStr(),
						"WEBRTC.SS",
						"REQUEST",
						"INFO",
						final_vhost_app_name.CStr(),
						stream->GetMediaSource().CStr(),
						remote_address->ToString(false).CStr());

			logti("URL %s is requested", stream->GetMediaSource().CStr());
		}
	}
	else
	{
		result = RequestStreamResult::local_success;
	}

	if (stream == nullptr)
	{
		logte("Cannot find stream (%s/%s)", final_vhost_app_name.CStr(), final_stream_name.CStr());
		return nullptr;
	}

	if(stream->WaitUntilStart(10000) == false)
	{
		logtw("(%s/%s) stream has not started.", final_vhost_app_name.CStr(), final_stream_name.CStr());
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

	auto session_description = std::make_shared<SessionDescription>(*stream->GetSessionDescription());
	session_description->SetOrigin("OvenMediaEngine", ov::Unique::GenerateUint32(), 2, "IN", 4, "127.0.0.1");
	session_description->SetIceUfrag(_ice_port->GenerateUfrag());
	session_description->Update();

	// Passed AccessControl
	ws_session->AddUserData("authorized", true);
	ws_session->AddUserData("new_url", parsed_url->ToUrlString(true));
	ws_session->AddUserData("stream_expired", session_life_time);

	return session_description;
}

// Called when receives an answer sdp from client
bool WebRtcPublisher::OnAddRemoteDescription(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
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

	// SignedPolicy and SignedToken
	auto request = ws_session->GetRequest();
	auto remote_address = request->GetRemote()->GetRemoteAddress();
	
	ov::String remote_sdp_text = peer_sdp->ToString();
	logtd("OnAddRemoteDescription: %s", remote_sdp_text.CStr());

	auto application = GetApplicationByName(final_vhost_app_name);
	auto stream = GetStream(final_vhost_app_name, final_stream_name);
	if (!stream)
	{
		logte("Cannot find stream (%s/%s)", final_vhost_app_name.CStr(), final_stream_name.CStr());
		return false;
	}

	auto session = RtcSession::Create(Publisher::GetSharedPtrAs<WebRtcPublisher>(), application, stream, offer_sdp, peer_sdp, _ice_port, ws_session);
	if (session != nullptr)
	{
		stream->AddSession(session);
		MonitorInstance->OnSessionConnected(*stream, PublisherType::Webrtc);

		auto ice_timeout = application->GetConfig().GetPublishers().GetWebrtcPublisher().GetTimeout();
		_ice_port->AddSession(IcePortObserver::GetSharedPtr(), session->GetId(), offer_sdp, peer_sdp, ice_timeout, session_life_time, session);

		// Session is created

		// Special purpose log
		stat_log(STAT_LOG_WEBRTC_EDGE_SESSION, "%s,%s,%s,%s,,,%s,%s,%u",
					 ov::Clock::Now().CStr(),
					 "WEBRTC.SS",
					 "SESSION",
					 "INFO",
					 "createClientSession",
					 stream->GetName().CStr(),
					 session->GetId());

		std::shared_ptr<info::Application> rtsp_live_app_info;
		std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
		std::shared_ptr<info::Application> rtsp_play_app_info;
		std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

		rtsp_live_app_metrics = nullptr;
		rtsp_play_app_metrics = nullptr;

		// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications 
		rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
		if (rtsp_live_app_info != nullptr)
		{
			rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
		}
		rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
		if (rtsp_play_app_info != nullptr)
		{
			rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
		}

		stat_log(STAT_LOG_WEBRTC_EDGE_SESSION, "%s,%s,%s,%s,,,%s:%d,%s:%d,%s,%u",
					ov::Clock::Now().CStr(),
					"WEBRTC.SS",
					"SESSION",
					"INFO",
					"Live",
					rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
					"Playback",
					rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0,
					stream->GetName().CStr(),
					session->GetId());
	}
	else
	{
		logte("Cannot create session");
	}

	return true;
}

bool WebRtcPublisher::OnStopCommand(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
									const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
									const std::shared_ptr<const SessionDescription> &offer_sdp,
									const std::shared_ptr<const SessionDescription> &peer_sdp)
{
	logti("Stop command received : %s/%s/%u", vhost_app_name.CStr(), stream_name.CStr(), offer_sdp->GetSessionId());

	ov::String uri { ws_session->GetRequest()->GetUri() };
	auto parsed_url { ov::Url::Parse(uri) };

	auto final_vhost_app_name = vhost_app_name;
	auto final_stream_name = stream_name;
	auto [new_url_exist, new_url] = ws_session->GetUserData("new_url");
	if (new_url_exist == true && std::holds_alternative<ov::String>(new_url) == true)
	{
		uri = std::get<ov::String>(new_url);

		parsed_url = ov::Url::Parse(uri);
		if (parsed_url == nullptr)
		{
			logte("Could not parse the url: %s", uri.CStr());
			return false;
		}

		final_vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(parsed_url->Host(), parsed_url->App());
		final_stream_name = parsed_url->Stream();
	}

	// Send Close to Admission Webhooks
	auto remote_address { ws_session->GetRequest()->GetRemote()->GetRemoteAddress() };
	if (parsed_url && remote_address)
	{
		SendCloseAdmissionWebhooks(parsed_url, remote_address);
	}
	// the return check is not necessary

	// Find Stream
	auto stream = std::static_pointer_cast<RtcStream>(GetStream(final_vhost_app_name, final_stream_name));
	if (!stream)
	{
		logte("To stop session failed. Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
		return false;
	}

	auto session = std::static_pointer_cast<RtcSession>(stream->GetSession(offer_sdp->GetSessionId()));
	if (session == nullptr)
	{
		logte("To stop session failed. Cannot find session by peer sdp session id (%u)", offer_sdp->GetSessionId());
		return false;
	}

	DisconnectSessionInternal(session);

	_ice_port->RemoveSession(session->GetId());

	return true;
}

bool WebRtcPublisher::OnIceCandidate(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session,
									 const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
									 const std::shared_ptr<RtcIceCandidate> &candidate,
									 const ov::String &username_fragment)
{
	return true;
}

/*
 * IcePort Implementation
 */

void WebRtcPublisher::OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data)
{
	logtd("IcePort OnStateChanged : %d", state);
	
	std::shared_ptr<RtcSession> session;
	try
	{
		session = std::any_cast<std::shared_ptr<RtcSession>>(user_data);	
	}
	catch(const std::bad_any_cast& e)
	{
		// Internal Error
		logtc("WebRtcPublisher::OnDataReceived - Could not convert user_data, internal error");
		return;
	}
	
	auto application = session->GetApplication();
	auto stream = std::static_pointer_cast<RtcStream>(session->GetStream());

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
			logti("IcePort is disconnected. : (%s/%s/%u) reason(%d)", stream->GetApplicationName(), stream->GetName().CStr(), session->GetId(), state);

			_signalling_server->Disconnect(session->GetApplication()->GetName(), session->GetStream()->GetName(), session->GetPeerSDP());

			//DisconnectSessionInternal(session);
			break;
		}
		default:
			break;
	}
}

void WebRtcPublisher::OnDataReceived(IcePort &port,uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data)
{
	std::shared_ptr<RtcSession> session;
	try
	{
		session = std::any_cast<std::shared_ptr<RtcSession>>(user_data);	
	}
	catch(const std::bad_any_cast& e)
	{
		// Internal Error
		logtc("WebRtcPublisher::OnDataReceived - Could not convert user_data, internal error");
		return;
	}

	logtd("WebRtcPublisher::OnDataReceived : %d", session_id);

	auto application = session->GetApplication();
	application->PushIncomingPacket(session, data);
}
