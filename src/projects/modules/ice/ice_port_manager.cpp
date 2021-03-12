//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "ice_port_manager.h"
#include "ice_private.h"

#include <modules/address/address_utilities.h>
#include <modules/rtc_signalling/rtc_ice_candidate.h>

std::shared_ptr<IcePort> IcePortManager::CreatePort(std::shared_ptr<IcePortObserver> observer)
{
	if(_ice_port == nullptr)
	{
		_ice_port = std::make_shared<IcePort>();

		observer->_id = _last_issued_observer_id++;
		_observers.push_back(observer);
	}
	
	return _ice_port;
}

bool IcePortManager::IsRegisteredObserver(const std::shared_ptr<IcePortObserver> &observer)
{
	for(const auto &item : _observers)
	{
		if(item->_id == observer->_id)
		{
			return true;
		}
	}

	return false;
}

const std::vector<RtcIceCandidate> &IcePortManager::GetIceCandidateList(const std::shared_ptr<IcePortObserver> &observer) const
{
	return observer->_ice_candidate_list;
}

bool IcePortManager::CreateIceCandidates(std::shared_ptr<IcePortObserver> observer, const cfg::bind::cmm::IceCandidates &ice_candidates_config)
{
	if(_ice_port == nullptr || IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	std::vector<RtcIceCandidate> ice_candidate_list;

	if(GenerateIceCandidates(ice_candidates_config, &ice_candidate_list) == false)
	{
		logte("Could not parse ICE candidates");
		return false;
	}

	bool is_parsed;
	auto ice_worker_count = ice_candidates_config.GetIceWorkerCount(&is_parsed);
	ice_worker_count = is_parsed ? ice_worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

	if(_ice_port->CreateIceCandidates(ice_candidate_list, ice_worker_count) == false)
	{
		Release(observer);

		logte("Could not create ice candidates");
		OV_ASSERT2(false);
	}
	else
	{
		observer->_ice_candidate_list.insert(observer->_ice_candidate_list.end(), ice_candidate_list.begin(), ice_candidate_list.end());
		logtd("IceCandidates is created successfully: %s", _ice_port->ToString().CStr());
	}

	return true;
}

bool IcePortManager::CreateTurnServer(std::shared_ptr<IcePortObserver> observer, uint16_t listening_port, const ov::SocketType socket_type, int tcp_relay_worker_count)
{
	if(_ice_port == nullptr || IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	if(_ice_port->CreateTurnServer(listening_port, socket_type, tcp_relay_worker_count) == false)
	{
		Release(observer);

		logte("Could not create turn server");
		OV_ASSERT2(false);
	}
	else
	{
		observer->_turn_server_socket_type = socket_type;
		observer->_turn_server_port = listening_port;
		logti("RelayServer is created successfully: host:%d?transport=tcp", listening_port);
	}
	
	return true;
}

bool IcePortManager::Release(std::shared_ptr<IcePortObserver> observer)
{
	if(IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	if(_ice_port != nullptr)
	{
		_ice_port->Close();
		_ice_port = nullptr;
	}

	_observers.clear();

	return true;
}

bool IcePortManager::GenerateIceCandidates(const cfg::bind::cmm::IceCandidates &ice_candidates, std::vector<RtcIceCandidate> *parsed_ice_candidate_list)
{
	auto &list = ice_candidates.GetIceCandidateList();

	for(auto &ice_candidate : list)
	{
		std::vector<ov::String> ip_list;
		ov::SocketType socket_type;
		int start_port;
		int end_port;

		if(ParseIceCandidate(ice_candidate, &ip_list, &socket_type, &start_port, &end_port) == false)
		{
			logte("Invalid ICE candidate in configuration: %s", ice_candidate.CStr());
			return false;
		}

		for(const auto &local_ip : ip_list)
		{
			for(int port_num = start_port; port_num <= end_port; port_num++)
			{
				// Create an ICE candidate using local_ip & port_num
				ov::String protocol = (socket_type == ov::SocketType::Tcp) ? "TCP" : "UDP";

				parsed_ice_candidate_list->emplace_back(protocol, local_ip, static_cast<uint16_t>(port_num), 0, "");
			}
		}
	}

	return true;
}

bool IcePortManager::ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, int *start_port, int *end_port)
{
	// Create SocketAddress instances from string
	//
	// Expression examples:
	//
	// - Case #1
	//   - Expression: 192.168.0.1:10000
	//   - Meaning: IP = 192.168.0.1, Port = 10000, Protocol: TCP
	//   - It generates single instance:
	//     [0] 192.168.0.1, 10000/TCP
	// - Case #2
	//   - Expression: 192.168.0.1:10000/udp
	//   - Meaning: IP = 192.168.0.1, Port = 10000, Protocol: UDP
	//   - It generates single instance:
	//     [0] 192.168.0.1, 10000/UDP
	// - Case #3
	//   - Expression: *:10000/udp
	//   - Meaning: IP = <All IP addresses>, Port = 10000, Protocol: UDP
	//   - It may generates multiple instances:
	//     [0] 192.168.0.1, 10000/UDP
	//     [1] 10.0.0.1, 10000/UDP
	// - Case #4
	//   - Expression: *:10000-10002/udp
	//   - Meaning: IP = <All IP addresses>, Port = Total 3 ports (10000 to 10002), Protocol: UDP
	//   - It may generates multiple instances:
	//     [0] 192.168.0.1, 10000/UDP
	//     [1] 192.168.0.1, 10001/UDP
	//     [2] 192.168.0.1, 10002/UDP
	//     [3] 10.0.0.1, 10000/UDP
	//     [4] 10.0.0.1, 10001/UDP
	//     [5] 10.0.0.1, 10002/UDP

	// <IP>:<Port range>[/<Protocol>]
	auto tokens = ice_candidate.Split(":");

	if(tokens.size() != 2)
	{
		return false;
	}

	// <IP>:<Port range>[/<Protocol>]
	// ~~~~
	ov::String ip = tokens[0];
	// <IP>:<Port range>[/<Protocol>]
	//      ~~~~~~~~~~~~~~~~~~~~~~~~~
	ov::String port_protocol = tokens[1];

	// Extract protocol
	tokens = port_protocol.Split("/");

	// <IP>:<Port range>[/<Protocol>]
	//      ~~~~~~~~~~~~
	ov::String port = tokens[0];

	ov::SocketType type = ov::SocketType::Tcp;
	int start = 0;
	int end = 0;

	switch(tokens.size())
	{
		case 2:
		{
			// Port with protocol
			// <IP>:<Port range>[/<Protocol>]
			//                    ~~~~~~~~~~
			ov::String protocol = tokens[1];
			protocol.MakeUpper();

			if(protocol == "UDP")
			{
				type = ov::SocketType::Udp;
			}
			else if(protocol == "TCP")
			{
				type = ov::SocketType::Tcp;
			}
			else
			{
				logte("Invalid ICE candidate found - not supported protocol: %s", tokens[1].CStr());
				return false;
			}

			[[fallthrough]];
		}

		case 1:
		{
			// Parse the port
			tokens = port.Split("-");

			switch(tokens.size())
			{
				case 1:
					// Single port
					start = ov::Converter::ToInt32(port);
					end = start;
					break;

				case 2:
					// Port range
					start = ov::Converter::ToInt32(tokens[0]);
					end = ov::Converter::ToInt32(tokens[1]);
					break;

				default:
					// Invalid port value
					logte("Invalid ICE candidate found - invalid port expression: %s", ice_candidate.CStr());
					return false;
			}

			break;
		}

		default:
			// Invalid expression
			logte("Invalid ICE candidate found - too many port expressions: %s", ice_candidate.CStr());
			return false;
	}

	if(
		(start <= 0) || (start >= 65535) ||
		(end <= 0) || (end >= 65535) ||
		(start > end)
		)
	{
		logte("Invalid ICE candidate found - invalid port range: %s", ice_candidate.CStr());
		return false;
	}

	if(ip == "*")
	{
		// multiple ip
		auto local_ip_list = ov::AddressUtilities::GetInstance()->GetIpList();

		ip_list->insert(ip_list->end(), local_ip_list.cbegin(), local_ip_list.cend());
	}
	else
	{
		ip_list->push_back(ip);
	}

	*socket_type = type;
	*start_port = start;
	*end_port = end;

	return true;
}