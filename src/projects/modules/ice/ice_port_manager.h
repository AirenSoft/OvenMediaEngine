//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_port.h"
#include "ice_port_observer.h"

#include <functional>
#include <memory>
#include <map>

#include <config/config.h>

class IcePortManager : public ov::Singleton<IcePortManager>
{
public:
	friend class ov::Singleton<IcePortManager>;

	~IcePortManager() override = default;

	std::shared_ptr<IcePort> CreatePort(std::shared_ptr<IcePortObserver> observer);
	bool CreateIceCandidates(std::shared_ptr<IcePortObserver> observer, const cfg::bind::cmm::IceCandidates &ice_candidates_config);
	bool CreateTurnServer(std::shared_ptr<IcePortObserver> observer, const ov::SocketAddress &listening, const ov::SocketType socket_type, int tcp_relay_worker_count);

	// TODO(Getroot): In the future, each IceCandidate and TurnServer can be released flexibly.
	// In the future, each IceCandidate and TurnServer can be released flexibly. 
	// Currently, WebRTC publisher and provider use IcePortManager until the server is shut down, 
	// so it is not a problem, but it is necessary if IcePortManager is used in dynamically 
	// created/deleted modules.
	bool Release(std::shared_ptr<IcePortObserver> observer);

protected:
	IcePortManager() = default;

	bool IsRegisteredObserver(const std::shared_ptr<IcePortObserver> &observer);
	bool GenerateIceCandidates(const cfg::bind::cmm::IceCandidates &ice_candidates_config, std::vector<std::vector<RtcIceCandidate>> *ice_candidate_list);
	bool ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, int *start_port, int *end_port);

private:
	std::shared_ptr<IcePort>	_ice_port = nullptr;
	std::atomic<uint32_t>		_last_issued_observer_id {0};
	std::vector<std::shared_ptr<IcePortObserver>>	_observers;
};
