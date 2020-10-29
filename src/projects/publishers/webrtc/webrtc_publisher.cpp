#include <utility>

#include "rtc_private.h"
#include "rtc_session.h"
#include "rtc_stream.h"
#include "webrtc_publisher.h"

#include "config/config_manager.h"

#include <orchestrator/orchestrator.h>

std::shared_ptr<WebRtcPublisher> WebRtcPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto webrtc = std::make_shared<WebRtcPublisher>(server_config, router);

	if (!webrtc->Start())
	{
		logte("An error occurred while creating WebRtcPublisher");
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
	auto webrtc_config = server_config.GetBind().GetPublishers().GetWebrtc();

	if (webrtc_config.IsParsed() == false)
	{
		logtw("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto &signalling_config = webrtc_config.GetSignalling();
	auto &port_config = signalling_config.GetPort();
	auto &tls_port_config = signalling_config.GetTlsPort();

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
	_signalling_server = std::make_shared<RtcSignallingServer>(server_config);
	_signalling_server->AddObserver(RtcSignallingObserver::GetSharedPtr());
	if (_signalling_server->Start(has_port ? &signalling_address : nullptr, has_tls_port ? &signalling_tls_address : nullptr) == false)
	{
		return false;
	}

	bool result = true;

	_ice_port = IcePortManager::GetInstance()->CreatePort(webrtc_config.GetIceCandidates(), IcePortObserver::GetSharedPtr());
	if (_ice_port == nullptr)
	{
		logte("Cannot initialize ICE Port. Check your ICE configuration");
		result = false;
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

		return false;
	}

	_message_thread.Start(ov::MessageThreadObserver<std::shared_ptr<ov::CommonMessage>>::GetSharedPtr());

	return Publisher::Start();
}

bool WebRtcPublisher::Stop()
{
	IcePortManager::GetInstance()->ReleasePort(_ice_port, IcePortObserver::GetSharedPtr());

	_signalling_server->RemoveObserver(RtcSignallingObserver::GetSharedPtr());
	_signalling_server->Stop();

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
	auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
	if (stream_metrics != nullptr)
	{
		stream_metrics->OnSessionDisconnected(PublisherType::Webrtc);
	}

	_ice_port->RemoveSession(session);
	session->Stop();

	StatLog(session->GetWSClient(), stream, session, RequestStreamResult::transfer_completed);

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

			DisconnectSessionInternal(session);
		}
		catch(const std::bad_any_cast& e) 
		{
			logtc("Wrong message!");
			return;
		}
	}
}

void WebRtcPublisher::StatLog(const std::shared_ptr<WebSocketClient> &ws_client,
							  const std::shared_ptr<RtcStream> &stream,
							  const std::shared_ptr<RtcSession> &session,
							  const RequestStreamResult &result)
{
	auto request = ws_client->GetClient()->GetRequest();
	auto remote = request->GetRemote();

	// logging for statistics
	// server domain yyyy-mm-dd tt:MM:ss url sent_bytes request_time upstream_cache_status http_status client_ip http_user_agent http_referer origin_addr origin_http_status geoip geoip_org http_encoding content_length upstream_connect_time upstream_header_time upstream_response_time
	// OvenMediaEngine 127.0.0.1       02-2020-08      01:18:21        /app/stream_o   -       -       -       200     127.0.0.1:57233 Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.130 Safari/537.36                -       -       -       -       -       -       -       -
	ov::String log;

	// Server Name
	log.AppendFormat("%s", _server_config.GetName().CStr());
	// Domain
	log.AppendFormat("\t%s", request->GetHeader("HOST").Split(":")[0].CStr());
	// Current Time
	std::time_t t = std::time(nullptr);
	char mbstr[100];
	memset(mbstr, 0, sizeof(mbstr));
	// yyyy-mm-dd
	std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d", std::localtime(&t));
	log.AppendFormat("\t%s", mbstr);
	// tt:mm:ss
	memset(mbstr, 0, sizeof(mbstr));
	std::strftime(mbstr, sizeof(mbstr), "%H:%M:%S", std::localtime(&t));
	log.AppendFormat("\t%s", mbstr);
	// Url
	log.AppendFormat("\t%s", request->GetRequestTarget().CStr());

	if (result == RequestStreamResult::transfer_completed && session != nullptr)
	{
		// Bytes sents
		log.AppendFormat("\t%llu", session->GetSentBytes());
		// request_time
		auto now = std::chrono::system_clock::now();
		std::chrono::duration<double, std::ratio<1>> elapsed = now - session->GetCreatedTime();
		log.AppendFormat("\t%f", elapsed.count());
	}
	else
	{
		// Bytes sents : -
		// request_time : -
		log.AppendFormat("\t-\t-");
	}

	// upstream_cache_status : -
	if (result == RequestStreamResult::local_success)
	{
		log.AppendFormat("\t%s", "hit");
	}
	else
	{
		log.AppendFormat("\t%s", "miss");
	}
	// http_status : 200 or 404
	log.AppendFormat("\t%s", stream != nullptr ? "200" : "404");
	// client_ip
	log.AppendFormat("\t%s", remote->GetRemoteAddress()->ToString().CStr());
	// http user agent
	log.AppendFormat("\t%s", request->GetHeader("User-Agent").CStr());
	// http referrer
	log.AppendFormat("\t%s", request->GetHeader("Referer").CStr());
	// origin addr
	// orchestrator->GetUrlListForLocation()

	if (result == RequestStreamResult::origin_success && stream != nullptr)
	{
		// origin http status : 200 or 404
		// geoip : -
		// geoip_org : -
		// http_encoding : -
		// content_length : -
		log.AppendFormat("\t%s\t-\t-\t-\t-", "200");
		// upstream_connect_time
		// upstream_header_time : -
		// upstream_response_time
		auto stream_metric = mon::Monitoring::GetInstance()->GetStreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
		if (stream_metric != nullptr)
		{
			log.AppendFormat("\t%f\t-\t%f", stream_metric->GetOriginRequestTimeMSec(), stream_metric->GetOriginResponseTimeMSec());
		}
		else
		{
			log.AppendFormat("\t-\t-\t-");
		}
	}
	else if (result == RequestStreamResult::origin_failed)
	{
		log.AppendFormat("\t%s\t-\t-\t-\t-", "404");
		log.AppendFormat("\t-\t-\t-");
	}
	else
	{
		log.AppendFormat("\t-\t-\t-\t-\t-");
		log.AppendFormat("\t-\t-\t-");
	}

	stat_log(STAT_LOG_WEBRTC_EDGE, "%s", log.CStr());
}


//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool WebRtcPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return _signalling_server->GetMonitoringCollectionData(collections);
}

// Publisher에서 Application 생성 요청이 온다.
std::shared_ptr<pub::Application> WebRtcPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
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
std::shared_ptr<const SessionDescription> WebRtcPublisher::OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client,
																		  const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
																		  std::vector<RtcIceCandidate> *ice_candidates)
{
	RequestStreamResult result = RequestStreamResult::init;
	auto request = ws_client->GetClient()->GetRequest();
	auto remote_address = request->GetRemote()->GetRemoteAddress();
	auto uri = request->GetUri();
	auto parsed_url = ov::Url::Parse(uri);

	logtc("uri : %s", uri.CStr());
	logtc("parsed : %s", parsed_url->ToUrlString().CStr());

	if (parsed_url == nullptr)
	{
		logte("Could not parse the url: %s", uri.CStr());
		return nullptr;
	}


	std::shared_ptr<const SignedToken> signed_token;
	auto signed_token_result = Publisher::HandleSignedToken(parsed_url, remote_address, signed_token);
	if(signed_token_result == Publisher::CheckSignatureResult::Error)
	{
		return nullptr;
	}
	else if(signed_token_result == Publisher::CheckSignatureResult::Fail)
	{
		logtw("%s", signed_token->GetErrMessage().CStr());
		return nullptr;
	}

	auto stream = std::static_pointer_cast<RtcStream>(GetStream(vhost_app_name, stream_name));
	if(stream == nullptr)
	{
		stream = std::dynamic_pointer_cast<RtcStream>(PullStream(parsed_url, vhost_app_name, host_name, stream_name));
		if(stream == nullptr)
		{
			result = RequestStreamResult::origin_failed;
		}
	}
	else
	{
		result = RequestStreamResult::local_success;
	}

	StatLog(ws_client, stream, nullptr, result);

	if (stream == nullptr)
	{
		return nullptr;
	}

	auto &candidates = _ice_port->GetIceCandidateList();
	ice_candidates->insert(ice_candidates->end(), candidates.cbegin(), candidates.cend());
	auto session_description = std::make_shared<SessionDescription>(*stream->GetSessionDescription());
	session_description->SetOrigin("OvenMediaEngine", ++_last_issued_session_id, 2, "IN", 4, "127.0.0.1");
	session_description->SetIceUfrag(_ice_port->GenerateUfrag());
	session_description->Update();

	return session_description;
}

// Called when receives an answer sdp from client
bool WebRtcPublisher::OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
											 const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
											 const std::shared_ptr<const SessionDescription> &offer_sdp,
											 const std::shared_ptr<const SessionDescription> &peer_sdp)
{
	auto application = GetApplicationByName(vhost_app_name);
	auto stream = GetStream(vhost_app_name, stream_name);
	if (!stream)
	{
		logte("Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
		return false;
	}

	// Signed URL
	auto request = ws_client->GetClient()->GetRequest();
	auto remote_address = request->GetRemote()->GetRemoteAddress();
	auto uri = request->GetUri();
	auto parsed_url = ov::Url::Parse(uri);

	if (parsed_url == nullptr)
	{
		logte("Could not parse the url: %s", uri.CStr());
		return false;
	}

	// These names are used for testing purposes
	uint64_t session_expired_time = 0;
	std::shared_ptr<const SignedToken> signed_token;
	auto signed_token_result = Publisher::HandleSignedToken(parsed_url, remote_address, signed_token);
	if(signed_token_result == Publisher::CheckSignatureResult::Error)
	{
		return false;
	}
	else if(signed_token_result == Publisher::CheckSignatureResult::Fail)
	{
		logtw("%s", signed_token->GetErrMessage().CStr());
		return false;
	}
	else if(signed_token_result == Publisher::CheckSignatureResult::Pass)
	{
		session_expired_time = signed_token->GetStreamExpiredTime();
	}

	ov::String remote_sdp_text = peer_sdp->ToString();
	logtd("OnAddRemoteDescription: %s", remote_sdp_text.CStr());

	auto session = RtcSession::Create(Publisher::GetSharedPtrAs<WebRtcPublisher>(), application, stream, offer_sdp, peer_sdp, _ice_port, ws_client);
	if (session != nullptr)
	{
		if(session_expired_time != 0)
		{
			session->SetSessionExpiredTime(session_expired_time);
		}

		stream->AddSession(session);
		auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
		if (stream_metrics != nullptr)
		{
			stream_metrics->OnSessionConnected(PublisherType::Webrtc);
		}

		_ice_port->AddSession(session, offer_sdp, peer_sdp);
	}
	else
	{
		logte("Cannot create session");
	}

	return true;
}

bool WebRtcPublisher::OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
									const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
									const std::shared_ptr<const SessionDescription> &offer_sdp,
									const std::shared_ptr<const SessionDescription> &peer_sdp)
{
	logtd("Stop commnad received : %s/%s/%u", vhost_app_name.CStr(), stream_name.CStr(), offer_sdp->GetSessionId());
	// Find Stream
	auto stream = std::static_pointer_cast<RtcStream>(GetStream(vhost_app_name, stream_name));
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

	return true;
}

// bitrate info(from signalling)
uint32_t WebRtcPublisher::OnGetBitrate(const std::shared_ptr<WebSocketClient> &ws_client,
									   const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name)
{
	auto stream = GetStream(vhost_app_name, stream_name);
	uint32_t bitrate = 0;

	if (!stream)
	{
		logte("Cannot find stream (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
		return 0;
	}

	auto tracks = stream->GetTracks();
	for (auto &track_iter : tracks)
	{
		MediaTrack *track = track_iter.second.get();

		if (track->GetCodecId() == common::MediaCodecId::Vp8 || track->GetCodecId() == common::MediaCodecId::Opus)
		{
			bitrate += track->GetBitrate();
		}
	}

	return bitrate;
}

bool WebRtcPublisher::OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
									 const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
									 const std::shared_ptr<RtcIceCandidate> &candidate,
									 const ov::String &username_fragment)
{
	return true;
}

/*
 * IcePort Implementation
 */

void WebRtcPublisher::OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session_info,
									 IcePortConnectionState state)
{
	logtd("IcePort OnStateChanged : %d", state);

	auto session = std::static_pointer_cast<RtcSession>(session_info);
	auto application = session->GetApplication();
	auto stream = std::static_pointer_cast<RtcStream>(session->GetStream());

	// state를 보고 session을 처리한다.
	switch (state)
	{
		case IcePortConnectionState::New:
		case IcePortConnectionState::Checking:
		case IcePortConnectionState::Connected:
		case IcePortConnectionState::Completed:
			// 연결되었을때는 할일이 없다.
			break;
		case IcePortConnectionState::Failed:
		case IcePortConnectionState::Disconnected:
		case IcePortConnectionState::Closed:
		{
			DisconnectSessionInternal(session);
			break;
		}
		default:
			break;
	}
}

void WebRtcPublisher::OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session_info, std::shared_ptr<const ov::Data> data)
{
	// ice_port를 통해 STUN을 제외한 모든 Packet이 들어온다.
	auto session = std::static_pointer_cast<pub::Session>(session_info);

	//받는 Data 형식을 협의해야 한다.
	auto application = session->GetApplication();
	application->PushIncomingPacket(session_info, data);
}
