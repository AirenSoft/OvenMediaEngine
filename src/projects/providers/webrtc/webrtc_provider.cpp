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
		: pvd::Provider(server_config, router)
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
		auto worker_count = signalling_config.GetWorker();

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

		if(IcePortManager::GetInstance()->CreateIceCandidates(IcePortObserver::GetSharedPtr(), webrtc_bind_config.GetIceCandidates()) == false)
		{
			logte("Could not create ICE Candidates. Check your ICE configuration");
			result = false;
		}
		
		bool tcp_relay_parsed = false;
		auto tcp_relay = webrtc_bind_config.GetIceCandidates().GetTcpRelay(&tcp_relay_parsed);
		if(tcp_relay_parsed)
		{
			auto items = tcp_relay.Split(":");
			if(items.size() != 2)
			{
				logte("TcpRelay format is incorrect : <Relay IP>:<Port>");
			}
			else
			{
				if(IcePortManager::GetInstance()->CreateTurnServer(IcePortObserver::GetSharedPtr(), std::atoi(items[1]), ov::SocketType::Tcp) == false)
				{
					logte("Could not create Turn Server. Check your configuration");
					result = false;
				}
			}
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
		return WebRTCApplication::Create(Provider::GetSharedPtrAs<Provider>(), application_info, _ice_port, _signalling_server);
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

		return application->GetOfferSDP();
	}

	bool WebRTCProvider::OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
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
		auto stream = WebRTCStream::Create(StreamSourceType::WebRTC);
		if(stream == nullptr)
		{
			logte("Could not create %s stream in %s application", stream_name.CStr(), vhost_app_name.CStr());
			return false;
		}
		
		application->AddStream(stream);

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
		return true;
	}

	//------------------------
	// IcePort
	//------------------------

	void WebRTCProvider::OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data)
	{

	}

	void WebRTCProvider::OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data)
	{

	}
}