#pragma once

#include "../base/common_types.h"
#include "../base/publisher/publisher.h"
#include "../base/media_route/media_route_application_interface.h"
#include "rtc_application.h"


class WebRtcPublisher : public Publisher,
                        public IcePortObserver,
                        public RtcSignallingObserver
{
public:
	static std::shared_ptr<WebRtcPublisher> Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router, std::shared_ptr<MediaRouteApplicationInterface> application);

	WebRtcPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router, std::shared_ptr<MediaRouteApplicationInterface> application);
	~WebRtcPublisher() override;

	// IcePortObserver Implementation

	// ICE 연결 상태가 바뀌면 통지해준다.
	void OnStateChanged(IcePort &port, const std::shared_ptr<SessionInfo> &session, IcePortConnectionState state) override;
	void OnDataReceived(IcePort &port, const std::shared_ptr<SessionInfo> &session, std::shared_ptr<const ov::Data> data) override;

	// SignallingObserver Implementation
	// 클라이언트가 Request Offer를 하면 다음 함수를 통해 SDP를 받아서 넘겨준다.
	std::shared_ptr<SessionDescription> OnRequestOffer(const ov::String &application_name,
	                                                   const ov::String &stream_name,
	                                                   std::vector<RtcIceCandidate> *ice_candidates) override;
	// 클라이언트가 자신의 SDP를 보내면 다음 함수를 호출한다.
	bool OnAddRemoteDescription(const ov::String &application_name,
	                            const ov::String &stream_name,
	                            const std::shared_ptr<SessionDescription> &offer_sdp,
	                            const std::shared_ptr<SessionDescription> &peer_sdp) override;
	// 클라이언트가 자신의 Ice Candidate를 보내면 다음 함수를 호출한다.
	// 이 함수를 통해 IcePortObserver에 Session ID와 candidates를 등록한다.
	// 향후 IcePort는 패킷을 받으면 해당 Session ID와 함께 Publisher에 전달하여 추적할 수 있게 한다.
	// OnAddRemoteIceCandidate 단계 없이 session id와, ICE ID/PASS를 바로 IcePort에 등록하여 추적하는 방식으로
	// 개발할수도 있다. IcePort->AddSession(session_info, ice_id, ice_pass);
	//bool OnAddRemoteIceCandidate(session_info, std::vector<std::shared_ptr<IceCandidate>> candidates);
	// 여기서 IcePort->AddSession(session_info, candidates)를 함
	// IcePort->SendPacket(session_info, packet);
	bool OnIceCandidate(const ov::String &application_name,
	                    const ov::String &stream_name,
	                    const std::shared_ptr<RtcIceCandidate> &candidate,
	                    const ov::String &username_fragment) override;

	bool OnStopCommand(const ov::String &application_name, const ov::String &stream_name,
	                   const std::shared_ptr<SessionDescription> &offer_sdp,
	                   const std::shared_ptr<SessionDescription> &peer_sdp) override;

    uint32_t OnGetBitrate(const ov::String &application_name, const ov::String &stream_name);

    bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) override;

private:
	bool Start() override;
	bool Stop() override;

	// Publisher Implementation
	cfg::PublisherType GetPublisherType() override
	{
		return cfg::PublisherType::Webrtc;
	}

	std::shared_ptr<Application> OnCreateApplication(const info::Application *application_info) override;

	std::shared_ptr<IcePort> _ice_port;
	std::shared_ptr<RtcSignallingServer> _signalling;
};
