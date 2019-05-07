//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtc_peer_info.h"

class RtcP2PManager
{
public:
	// Create a PeerInfo from user-agent
	std::shared_ptr<RtcPeerInfo> CreatePeerInfo(peer_id_t id, const std::shared_ptr<WebSocketClient> &response);

	// Add to _peer_list
	std::shared_ptr<RtcPeerInfo> FindPeer(peer_id_t peer_id);
	bool RemovePeer(const std::shared_ptr<RtcPeerInfo> &peer, size_t max_clients_per_host);

	bool RegisterAsHostPeer(const std::shared_ptr<RtcPeerInfo> &peer);
	std::shared_ptr<RtcPeerInfo> TryToRegisterAsClientPeer(const std::shared_ptr<RtcPeerInfo> &peer, size_t max_clients_per_host);

	std::shared_ptr<RtcPeerInfo> GetClientPeerOf(const std::shared_ptr<RtcPeerInfo> &host, peer_id_t client_id);
	std::map<peer_id_t, std::shared_ptr<RtcPeerInfo>> GetClientPeerList(const std::shared_ptr<RtcPeerInfo> &host);

	int GetPeerCount() const;
	int GetClientPeerCount() const;

protected:

	std::recursive_mutex _list_mutex;

	// All peer list
	// key: peer id, value: peer info list
	std::map<peer_id_t, std::shared_ptr<RtcPeerInfo>> _peer_list;

	// List of hosts that can accept client
	// key: host id, value: host info
	std::map<peer_id_t, std::shared_ptr<RtcPeerInfo>> _available_list;

	int _total_client_count = 0;
};
