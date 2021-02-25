//==============================================================================
//
//  RtmpProvider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "webrtc_provider.h"

namespace pvd
{
	std::shared_ptr<WebRTCProvider> WebRTCProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	{
		return nullptr;
	}

	WebRTCProvider::WebRTCProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: pvd::Provider(server_config, router)
	{

	}

	WebRTCProvider::~WebRTCProvider()
	{

	}

	//------------------------
	// Provider
	//------------------------

	bool WebRTCProvider::Start()
	{
		return true;
	}

	bool WebRTCProvider::Stop()
	{
		return true;
	}

	std::shared_ptr<pvd::Application> WebRTCProvider::OnCreateProviderApplication(const info::Application &application_info)
	{
		return nullptr;
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
		return nullptr;
	}

	bool WebRTCProvider::OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp)
	{
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

	void WebRTCProvider::OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session, IcePortConnectionState state)
	{

	}

	void WebRTCProvider::OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session, std::shared_ptr<const ov::Data> data)
	{

	}
}