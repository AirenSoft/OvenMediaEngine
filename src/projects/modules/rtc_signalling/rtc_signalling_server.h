//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "modules/rtc_signalling/p2p/rtc_p2p_manager.h"
#include "rtc_ice_candidate.h"
#include "rtc_signalling_observer.h"

#include <memory>

#include <base/info/host.h>
#include <base/media_route/media_route_interface.h>
#include <base/publisher/publisher.h>
#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>
#include <media_router/media_route_application.h>
#include <modules/ice/ice.h>

class RtcSignallingServer : public ov::EnableSharedFromThis<RtcSignallingServer>
{
public:
	RtcSignallingServer(const cfg::Server &server_config);
	~RtcSignallingServer() override = default;

	bool Start(const ov::SocketAddress *address, const ov::SocketAddress *tls_address);
	bool Stop();

	bool AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer);

	bool Disconnect(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &peer_sdp);

	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &stream_collections);

	int GetTotalPeerCount() const;
	int GetClientPeerCount() const;

protected:
	struct RtcSignallingInfo
	{
		RtcSignallingInfo(ov::String host_name, ov::String app_name, ov::String stream_name,
						  ov::String internal_app_name,
						  peer_id_t id, std::shared_ptr<RtcPeerInfo> peer_info,
						  std::shared_ptr<SessionDescription> offer_sdp, std::shared_ptr<SessionDescription> peer_sdp,
						  std::vector<RtcIceCandidate> local_candidates, std::vector<RtcIceCandidate> remote_candidates)
			: host_name(host_name),
			  app_name(std::move(app_name)),
			  stream_name(std::move(stream_name)),
			  internal_app_name(internal_app_name),
			  id(id),
			  peer_info(std::move(peer_info)),
			  offer_sdp(std::move(offer_sdp)),
			  peer_sdp(std::move(peer_sdp)),
			  local_candidates(std::move(local_candidates)),
			  remote_candidates(std::move(remote_candidates))
		{
		}

		ov::String host_name;
		ov::String app_name;
		ov::String stream_name;

		ov::String internal_app_name;

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
	};

	using SdpCallback = std::function<void(std::shared_ptr<SessionDescription> sdp, std::shared_ptr<ov::Error> error)>;

	bool InitializeWebSocketServer();

	std::shared_ptr<ov::Error> DispatchCommand(const std::shared_ptr<WebSocketClient> &ws_client, const ov::String &command, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<const WebSocketFrame> &message);
	std::shared_ptr<ov::Error> DispatchRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchAnswer(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidate(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchOfferP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchCandidateP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info);
	std::shared_ptr<ov::Error> DispatchStop(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info);

	const cfg::Server _server_config;

	std::shared_ptr<HttpServer> _http_server;
	std::shared_ptr<HttpsServer> _https_server;

	std::vector<std::shared_ptr<RtcSignallingObserver>> _observers;

	std::map<peer_id_t, std::shared_ptr<RtcSignallingInfo>> _client_list;
	std::mutex _client_list_mutex;

	const cfg::P2P _p2p_info;
	RtcP2PManager _p2p_manager;
};
