#pragma once

#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/mediarouter/media_route_application_interface.h"
#include "base/ovlibrary/message_thread.h"
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

	// ICE 연결 상태가 바뀌면 통지해준다.
	void OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session, IcePortConnectionState state) override;
	void OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session, std::shared_ptr<const ov::Data> data) override;

	// SignallingObserver Implementation
	// 클라이언트가 Request Offer를 하면 다음 함수를 통해 SDP를 받아서 넘겨준다.
	std::shared_ptr<const SessionDescription> OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client,
													   const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
													   std::vector<RtcIceCandidate> *ice_candidates) override;
	// 클라이언트가 자신의 SDP를 보내면 다음 함수를 호출한다.
	bool OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
								const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
								const std::shared_ptr<const SessionDescription> &offer_sdp,
								const std::shared_ptr<const SessionDescription> &peer_sdp) override;
	// 클라이언트가 자신의 Ice Candidate를 보내면 다음 함수를 호출한다.
	// 이 함수를 통해 IcePortObserver에 Session ID와 candidates를 등록한다.
	// 향후 IcePort는 패킷을 받으면 해당 Session ID와 함께 Publisher에 전달하여 추적할 수 있게 한다.
	// OnAddRemoteIceCandidate 단계 없이 session id와, ICE ID/PASS를 바로 IcePort에 등록하여 추적하는 방식으로
	// 개발할수도 있다. IcePort->AddSession(session_info, ice_id, ice_pass);
	//bool OnAddRemoteIceCandidate(session_info, std::vector<std::shared_ptr<IceCandidate>> candidates);
	// 여기서 IcePort->AddSession(session_info, candidates)를 함
	// IcePort->SendPacket(session_info, packet);
	bool OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
						const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
	                    const std::shared_ptr<RtcIceCandidate> &candidate,
	                    const ov::String &username_fragment) override;

	bool OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
					   const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name,
	                   const std::shared_ptr<const SessionDescription> &offer_sdp,
	                   const std::shared_ptr<const SessionDescription> &peer_sdp) override;

    uint32_t OnGetBitrate(const std::shared_ptr<WebSocketClient> &ws_client,
						  const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name) override;

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

	std::atomic<session_id_t> _last_issued_session_id { 100 };

	void StatLog(const std::shared_ptr<WebSocketClient> &ws_client, 
				const std::shared_ptr<RtcStream> &stream, 
				const std::shared_ptr<RtcSession> &session, 
				const RequestStreamResult &result);
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
};
