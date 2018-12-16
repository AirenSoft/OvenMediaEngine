//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtc_signalling_observer.h"
#include "rtc_ice_candidate.h"

#include <memory>

#include <base/application/application.h>
#include <base/media_route/media_route_interface.h>
#include <ice/ice.h>
#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>
#include <media_router/media_route_application.h>
#include <relay/relay.h>

class RtcSignallingServer : public ov::EnableSharedFromThis<RtcSignallingServer>
{
public:
	RtcSignallingServer(const info::Application &application_info, std::shared_ptr<MediaRouteApplicationInterface> application);
	virtual ~RtcSignallingServer() = default;

	bool Start(const ov::SocketAddress &address, const std::shared_ptr<Certificate> &certificate = nullptr);
	bool Stop();

	bool AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer);

	bool Disconnect(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &peer_sdp);

protected:
	struct RtcSignallingInfo
	{
		ov::String application_name;
		ov::String stream_name;

		// signalling server에서 발급한 id
		ov::String id;

		// client가 연결 한 뒤 request offer 했을 때 보내준 offer SDP
		std::shared_ptr<SessionDescription> offer_sdp;

		// client의 sdp
		std::shared_ptr<SessionDescription> peer_sdp;

		// server의 candidates
		std::vector<RtcIceCandidate> local_candidates;

		// client의 candidates
		std::vector<RtcIceCandidate> remote_candidates;
	};

	using SdpCallback = std::function<void(std::shared_ptr<SessionDescription> sdp, std::shared_ptr<ov::Error> error)>;

	void ProcessCommand(const ov::String &command, const ov::JsonObject &object, RtcSignallingInfo &info, const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const WebSocketFrame> &message);
	void ProcessRequestOffer(RtcSignallingInfo &info, const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const WebSocketFrame> &message, SdpCallback callback);

	const info::Application &_application_info;
	std::shared_ptr<MediaRouteApplicationInterface> _application;

	std::shared_ptr<HttpServer> _http_server;

	std::vector<std::shared_ptr<RtcSignallingObserver>> _observers;

	ov::DelayQueue _sdp_timer;

	// key: response
	// value: sdp
	std::map<std::shared_ptr<WebSocketResponse>, RtcSignallingInfo> _client_list;
};
