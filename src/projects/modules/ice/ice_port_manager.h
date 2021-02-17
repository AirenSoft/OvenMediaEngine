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
	bool CreateIceCandidates(std::shared_ptr<IcePort> ice_port, const cfg::bind::pub::IceCandidates &ice_candidates);
	bool CreateTurnServer(std::shared_ptr<IcePort> ice_port, uint16_t listening_port, const ov::SocketType socket_type);
	bool ReleasePort(std::shared_ptr<IcePort> ice_port, std::shared_ptr<IcePortObserver> observer);

protected:
	IcePortManager() = default;

	bool GenerateIceCandidates(const cfg::bind::pub::IceCandidates &ice_candidates, std::vector<RtcIceCandidate> *parsed_ice_candidate_list);
	bool ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, int *start_port, int *end_port);
};
