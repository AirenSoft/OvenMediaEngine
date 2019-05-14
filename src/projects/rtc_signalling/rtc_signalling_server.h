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
#include "p2p/rtc_p2p_manager.h"

#include <memory>

#include <base/application/application.h>
#include <base/media_route/media_route_interface.h>
#include <ice/ice.h>
#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>
#include <media_router/media_route_application.h>
#include <relay/relay.h>
#include "../base/publisher/publisher.h"

class RtcSignallingServer : public ov::EnableSharedFromThis<RtcSignallingServer>
{
public:
	RtcSignallingServer(const info::Application *application_info, std::shared_ptr<MediaRouteApplicationInterface> application);
	~RtcSignallingServer() override = default;

	bool Start(const ov::SocketAddress &address);
	bool Stop();

	bool AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer);

	bool Disconnect(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &peer_sdp);

   	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &stream_collections);

	int GetTotalPeerCount() const;
	int GetClientPeerCount() const;
	
protected:
	struct RtcSignallingInfo
	{
		ov::String application_name;
		ov::String stream_name;

		// signalling server에서 발급한 id
		// WebSocket 접속만 되어 있고, request offer하지 않은 상태에서는 P2P_INVALID_PEER_ID 로 되어 있음
		peer_id_t id = P2P_INVALID_PEER_ID;
		bool peer_was_client = false;

		std::shared_ptr<RtcPeerInfo> peer_info;

		// client가 연결 한 뒤 request offer 했을 때 보내준 offer SDP
		std::shared_ptr<SessionDescription> offer_sdp;

		// client의 sdp
		std::shared_ptr<SessionDescription> peer_sdp;

		// server의 candidates
		std::vector<RtcIceCandidate> local_candidates;

		// client의 candidates
		std::vector<RtcIceCandidate> remote_candidates;

		RtcSignallingInfo(ov::String application_name, ov::String stream_name,
		                  peer_id_t id, std::shared_ptr<RtcPeerInfo> peer_info,
		                  std::shared_ptr<SessionDescription> offer_sdp, std::shared_ptr<SessionDescription> peer_sdp,
		                  std::vector<RtcIceCandidate> local_candidates, std::vector<RtcIceCandidate> remote_candidates)
			: application_name(std::move(application_name)),
			  stream_name(std::move(stream_name)),
			  id(id),
			  peer_info(std::move(peer_info)),
			  offer_sdp(std::move(offer_sdp)),
			  peer_sdp(std::move(peer_sdp)),
			  local_candidates(std::move(local_candidates)),
			  remote_candidates(std::move(remote_candidates))
		{
		}
	};

	using SdpCallback = std::function<void(std::shared_ptr<SessionDescription> sdp, std::shared_ptr<ov::Error> error)>;

	bool InitializeWebSocketServer();

	std::shared_ptr<ov::Error> DispatchCommand(const ov::String &command, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<WebSocketClient> &response, const std::shared_ptr<const WebSocketFrame> &message);
	std::shared_ptr<ov::Error> DispatchRequestOffer(std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<WebSocketClient> &response);
	std::shared_ptr<ov::Error> DispatchAnswer(const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidate(const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchOfferP2P(const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidateP2P(const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchStop(std::shared_ptr<RtcSignallingInfo> &info);

	const info::Application *_application_info;
	const cfg::WebrtcPublisher *_webrtc_publisher_info;
	const cfg::P2P *_p2p_info;

	std::shared_ptr<MediaRouteApplicationInterface> _application;

	std::shared_ptr<HttpServer> _http_server;

	std::vector<std::shared_ptr<RtcSignallingObserver>> _observers;

	std::map<peer_id_t, std::shared_ptr<RtcSignallingInfo>> _client_list;
	std::mutex _client_list_mutex;

	RtcP2PManager _p2p_manager;
};
