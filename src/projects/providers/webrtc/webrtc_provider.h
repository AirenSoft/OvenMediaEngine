//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/provider/provider.h"
#include "base/common_types.h"

#include "modules/ice/ice_port_manager.h"
#include "modules/rtc_signalling/rtc_signalling.h"

namespace pvd
{
	class WebRTCProvider : public pvd::Provider,
						   public IcePortObserver,
						   public RtcSignallingObserver
	{
	public:
		static std::shared_ptr<WebRTCProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

		explicit WebRTCProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		~WebRTCProvider() override;

		bool Start() override;
		bool Stop() override;

		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
		ProviderStreamDirection GetProviderStreamDirection() const override
		{
			return ProviderStreamDirection::Push;
		}

		ProviderType GetProviderType() const override
		{
			return ProviderType::WebRTC;
		}

		const char* GetProviderName() const override
		{
			return "WebRTCProvider";
		}

		//--------------------------------------------------------------------
		// IcePortObserver Implementation
		//--------------------------------------------------------------------
		void OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session, IcePortConnectionState state) 	override;
		void OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session, std::shared_ptr<const ov::Data> data) override;

		//--------------------------------------------------------------------
		// SignallingObserver Implementation
		//--------------------------------------------------------------------
		std::shared_ptr<const SessionDescription> OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client,
														const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
														std::vector<RtcIceCandidate> *ice_candidates, bool &tcp_relay) override;
		bool OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
									const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
									const std::shared_ptr<const SessionDescription> &offer_sdp,
									const std::shared_ptr<const SessionDescription> &peer_sdp) override;
		bool OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
							const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
							const std::shared_ptr<RtcIceCandidate> &candidate,
							const ov::String &username_fragment) override;

		bool OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
						const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
						const std::shared_ptr<const SessionDescription> &offer_sdp,
						const std::shared_ptr<const SessionDescription> &peer_sdp) override;
	private:
		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
		std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &application_info) override;
		bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;

	};
}