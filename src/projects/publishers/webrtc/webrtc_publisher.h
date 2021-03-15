#pragma once

#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"
#include "base/ovlibrary/message_thread.h"
#include "base/ovlibrary/delay_queue.h"
#include "rtc_application.h"
#include <orchestrator/orchestrator.h>

class WebRtcPublisher : public pub::Publisher,
                        public IcePortObserver,
                        public RtcSignallingObserver,
						public ov::MessageThreadObserver<std::shared_ptr<ov::CommonMessage>>
{
public:
	static std::shared_ptr<WebRtcPublisher> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	WebRtcPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~WebRtcPublisher() override;

	bool Stop() override;
	bool DisconnectSession(const std::shared_ptr<RtcSession> &session);

	// MessageThread Implementation
	void OnMessage(const std::shared_ptr<ov::CommonMessage> &message) override;

	// IcePortObserver Implementation
	void OnStateChanged(IcePort &port, uint32_t session_id, IcePortConnectionState state, std::any user_data) override;
	void OnDataReceived(IcePort &port, uint32_t session_id, std::shared_ptr<const ov::Data> data, std::any user_data) override;

	// SignallingObserver Implementation
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
	enum class MessageCode : uint32_t
	{
		DISCONNECT_SESSION = 1,
	};

	enum class RequestStreamResult : int8_t
	{
		init = 0,
		local_success,
		local_failed,
		origin_success,
		origin_failed,
		transfer_completed,
	};

	bool Start() override;
	bool DisconnectSessionInternal(const std::shared_ptr<RtcSession> &session);

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	PublisherType GetPublisherType() const override
	{
		return PublisherType::Webrtc;
	}
	const char *GetPublisherName() const override
	{
		return "WebRTC Publisher";
	}

	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application) override;

	std::shared_ptr<IcePort> _ice_port;
	std::shared_ptr<RtcSignallingServer> _signalling_server;
	ov::MessageThread<std::shared_ptr<ov::CommonMessage>>	_message_thread;

	// for special purpose log
	ov::DelayQueue _timer;
};
