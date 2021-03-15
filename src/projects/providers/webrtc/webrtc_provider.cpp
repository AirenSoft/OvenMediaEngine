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
			logte("An error occurred while creating WebRTCProvider");
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
			logtw("%s is disabled by configuration", GetProviderName());
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
		_signalling_server = std::make_shared<RtcSignallingServer>(server_config);
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
				bool is_parsed;
				auto tcp_relay_worker_count = ice_candidates_config.GetTcpRelayWorkerCount(&is_parsed);
				tcp_relay_worker_count = is_parsed ? tcp_relay_worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

				if(IcePortManager::GetInstance()->CreateTurnServer(IcePortObserver::GetSharedPtr(), std::atoi(items[1]), ov::SocketType::Tcp, tcp_relay_worker_count) == false)
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

		_signalling_server->RemoveObserver(RtcSignallingObserver::GetSharedPtr());
		_signalling_server->Stop();

		return Provider::Stop();
	}

	std::shared_ptr<pvd::Application> WebRTCProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		return WebRTCApplication::Create(PushProvider::GetSharedPtrAs<PushProvider>(), application_info, _certificate, _ice_port, _signalling_server);
	}

	bool WebRTCProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
	{
		return true;
	}

	//------------------------
	// Signalling
	//------------------------

	std::shared_ptr<const SessionDescription> WebRTCProvider::OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client,
													const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
													std::vector<RtcIceCandidate> *ice_candidates, bool &tcp_relay)
	{
		logtd("WebRTCProvider::OnAddRemoteDescription");
		auto request = ws_client->GetClient()->GetRequest();
		auto remote_address = request->GetRemote()->GetRemoteAddress();
		auto uri = request->GetUri();
		auto parsed_url = ov::Url::Parse(uri);
		if (parsed_url == nullptr)
		{
			logte("Could not parse the url: %s", uri.CStr());
			return nullptr;
		}

		// TODO(Getroot): Implement SingedPolicy
			
		// Check if same stream name is exist
		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(vhost_app_name));
		if(application == nullptr)
		{
			logte("Could not find %s application", vhost_app_name.CStr());
			return nullptr;
		}
		
		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if(application->GetStreamByName(stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", stream_name.CStr(), vhost_app_name.CStr());
			return nullptr;
		}

		auto transport = parsed_url->GetQueryValue("transport");
		if(transport.UpperCaseString() == "TCP")
		{
			tcp_relay = true;
		}

		auto &candidates = IcePortManager::GetInstance()->GetIceCandidateList(IcePortObserver::GetSharedPtr());
		ice_candidates->insert(ice_candidates->end(), candidates.cbegin(), candidates.cend());
		auto session_description = std::make_shared<SessionDescription>(*application->GetOfferSDP());
		session_description->SetOrigin("OvenMediaEngine", ov::Unique::GenerateUint32(), 2, "IN", 4, "127.0.0.1");
		session_description->SetIceUfrag(_ice_port->GenerateUfrag());
		session_description->Update();

		return session_description;
	}

	bool WebRTCProvider::OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
		logtd("WebRTCProvider::OnAddRemoteDescription");

		// TODO(Getroot): Implement SingedPolicy

		// Check if same stream name is exist
		auto application = std::dynamic_pointer_cast<WebRTCApplication>(GetApplicationByName(vhost_app_name));
		if(application == nullptr)
		{
			logte("Could not find %s application", vhost_app_name.CStr());
			return false;
		}
		
		std::lock_guard<std::mutex> lock(_stream_lock);

		// TODO(Getroot): Implement BlockDuplicateStreamName option
		if(application->GetStreamByName(stream_name) != nullptr)
		{
			logte("%s stream is already exist in %s application", stream_name.CStr(), vhost_app_name.CStr());
			return false;
		}

		// Create Stream
		auto channel_id = offer_sdp->GetSessionId();
		auto stream = WebRTCStream::Create(StreamSourceType::WebRTC, stream_name, channel_id, PushProvider::GetSharedPtrAs<PushProvider>(), offer_sdp, peer_sdp, _certificate, _ice_port);
		if(stream == nullptr)
		{
			logte("Could not create %s stream in %s application", stream_name.CStr(), vhost_app_name.CStr());
			return false;
		}
		
		_ice_port->AddSession(IcePortObserver::GetSharedPtr(), stream->GetId(), offer_sdp, peer_sdp, 30*1000, stream);

		if(OnChannelCreated(channel_id, stream) == false)
		{
			return false;
		}

		// The stream of the webrtc provider has already completed signaling at this point.
		if(PublishChannel(channel_id, vhost_app_name, stream) == false)
		{
			return false;
		}

		return true;
	}

	bool WebRTCProvider::OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
						const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
						const std::shared_ptr<RtcIceCandidate> &candidate,
						const ov::String &username_fragment)
	{
		return true;
	}

	bool WebRTCProvider::OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
					const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
					const std::shared_ptr<const SessionDescription> &offer_sdp,
					const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
		logti("Stop commnad received : %s/%s/%u", vhost_app_name.CStr(), stream_name.CStr(), offer_sdp->GetSessionId());

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
			logte("Cannot create certificate: %s", error->ToString().CStr());
			return nullptr;
		}

		return certificate;
	}

	std::shared_ptr<Certificate> WebRTCProvider::GetCertificate()
	{
		return _certificate;
	}
}