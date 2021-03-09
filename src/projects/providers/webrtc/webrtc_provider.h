//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/provider/push_provider/provider.h"
#include "modules/ice/ice_port_manager.h"
#include "modules/rtc_signalling/rtc_signalling.h"
#include "base/ovlibrary/ovlibrary.h"
#include "orchestrator/orchestrator.h"

namespace pvd
{
	class WebRTCProvider : public PushProvider,
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

		//--------------------------------------------------------------------
		// IcePortObserver Implementation
		//--------------------------------------------------------------------
		void OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data) 	override;
		void OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data) override;
		//--------------------------------------------------------------------

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
		//--------------------------------------------------------------------
	private:
		std::shared_ptr<Certificate> CreateCertificate();
		std::shared_ptr<Certificate> GetCertificate();

		//--------------------------------------------------------------------
		// Implementation of Provider's pure virtual functions
		//--------------------------------------------------------------------
		std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &application_info) override;
		bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;
		//--------------------------------------------------------------------

		std::shared_ptr<IcePort> _ice_port = nullptr;
		std::shared_ptr<RtcSignallingServer> _signalling_server = nullptr;
		std::shared_ptr<Certificate> _certificate = nullptr;

		std::mutex	_stream_lock;
	};
}