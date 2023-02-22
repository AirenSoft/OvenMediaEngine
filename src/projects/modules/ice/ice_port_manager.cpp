//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "ice_port_manager.h"

#include <modules/address/address_utilities.h>
#include <modules/rtc_signalling/rtc_ice_candidate.h>

#include "ice_private.h"

std::shared_ptr<IcePort> IcePortManager::CreatePort(std::shared_ptr<IcePortObserver> observer)
{
	if (_ice_port == nullptr)
	{
		_ice_port = std::make_shared<IcePort>();

		observer->SetId(_last_issued_observer_id++);
		_observers.push_back(observer);
	}

	return _ice_port;
}

bool IcePortManager::IsRegisteredObserver(const std::shared_ptr<IcePortObserver> &observer)
{
	for (const auto &item : _observers)
	{
		if (item->GetId() == observer->GetId())
		{
			return true;
		}
	}

	return false;
}

bool IcePortManager::CreateIceCandidates(std::shared_ptr<IcePortObserver> observer, const cfg::bind::cmm::IceCandidates &ice_candidates_config)
{
	if (_ice_port == nullptr || IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	RtcIceCandidateList ice_candidate_list;

	if (GenerateIceCandidates(ice_candidates_config, &ice_candidate_list) == false)
	{
		logte("Could not parse ICE candidates");
		return false;
	}

	bool is_parsed;
	auto ice_worker_count = ice_candidates_config.GetIceWorkerCount(&is_parsed);
	ice_worker_count = is_parsed ? ice_worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

	if (_ice_port->CreateIceCandidates(ice_candidate_list, ice_worker_count) == false)
	{
		Release(observer);

		logte("Could not create ice candidates");
		OV_ASSERT2(false);
	}
	else
	{
		observer->AppendIceCandidates(ice_candidate_list);
		logtd("IceCandidates are created successfully: %s", _ice_port->ToString().CStr());
	}

	return true;
}

bool IcePortManager::CreateTurnServer(std::shared_ptr<IcePortObserver> observer, const ov::SocketAddress &listening, const ov::SocketType socket_type, int tcp_relay_worker_count)
{
	if (_ice_port == nullptr || IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	if (_ice_port->CreateTurnServer(listening, socket_type, tcp_relay_worker_count) == false)
	{
		Release(observer);

		logte("Could not create turn server");
		return false;
	}
	else
	{
		observer->SetTurnServerSocketType(socket_type);
		observer->SetTurnServerPort(listening.Port());
		logti("RelayServer is created successfully: host:%d?transport=tcp", listening.Port());
	}

	return true;
}

bool IcePortManager::Release(std::shared_ptr<IcePortObserver> observer)
{
	if (IsRegisteredObserver(observer) == false)
	{
		return false;
	}

	if (_ice_port != nullptr)
	{
		_ice_port->Close();
		_ice_port = nullptr;
	}

	_observers.clear();

	return true;
}

bool IcePortManager::GenerateIceCandidates(const cfg::bind::cmm::IceCandidates &ice_candidates_config, RtcIceCandidateList *ice_candidate_list)
{
	auto &list_config = ice_candidates_config.GetIceCandidateList();

	for (auto &ice_candidate_config : list_config)
	{
		std::vector<ov::String> ip_list;
		ov::SocketType socket_type;
		std::vector<ov::SocketAddress::PortRange> port_range_list;

		if (ParseIceCandidate(ice_candidate_config, &ip_list, &socket_type, &port_range_list) == false)
		{
			return false;
		}

		ov::String protocol = StringFromSocketType(socket_type);
		std::vector<ov::SocketAddress> address_list;

		for (const auto &port_range : port_range_list)
		{
			for (int port_num = port_range.start_port; port_num <= port_range.end_port; port_num++)
			{
				std::vector<RtcIceCandidate> ice_candidates;

				for (const auto &local_ip : ip_list)
				{
					try
					{
						address_list = ov::SocketAddress::Create(local_ip, static_cast<uint16_t>(port_num));
					}
					catch (const ov::Error &e)
					{
						logte("Could not create socket address: %s", e.What());
						return false;
					}

					for (const auto &address : address_list)
					{
						// Create an ICE candidate using local_ip & port_num
						RtcIceCandidate ice_candidate(protocol, address, 0, "");

						logtd("ICE Candidate is created: %s (from %s)", ice_candidate.ToString().CStr(), address.ToString().CStr());

						ice_candidates.emplace_back(std::move(ice_candidate));
					}
				}

				ice_candidate_list->push_back(std::move(ice_candidates));
			}
		}
	}

	return true;
}

bool IcePortManager::ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, std::vector<ov::SocketAddress::PortRange> *port_range_list)
{
	// Create SocketAddress instances from string
	//
	// Expression examples:
	//
	// - Case #1
	//   - Expression: 192.168.0.1:10000
	//   - Meaning: IP = 192.168.0.1, Port = 10000, Protocol: TCP
	//   - It generates a instance:
	//     [0] 192.168.0.1, 10000/TCP
	// - Case #2
	//   - Expression: 192.168.0.1:10000/udp
	//   - Meaning: IP = 192.168.0.1, Port = 10000, Protocol: UDP
	//   - It generates a instance:
	//     [0] 192.168.0.1, 10000/UDP
	// - Case #3
	//   - Expression: *:10000/udp
	//   - Meaning: IP = <All IPv4 addresses>, Port = 10000, Protocol: UDP
	//   - It may generates multiple instances:
	//     [0] 192.168.0.1, 10000/UDP
	//     [1] 10.0.0.1, 10000/UDP
	// - Case #4
	//   - Expression: *:10000-10002/udp
	//   - Meaning: IP = <All IPv4 addresses>, Port = Total 3 ports (10000 to 10002), Protocol: UDP
	//   - It may generates multiple instances:
	//     [0] 192.168.0.1, 10000/UDP
	//     [1] 192.168.0.1, 10001/UDP
	//     [2] 192.168.0.1, 10002/UDP
	//     [3] 10.0.0.1, 10000/UDP
	//     [4] 10.0.0.1, 10001/UDP
	//     [5] 10.0.0.1, 10002/UDP
	// - Case #5
	//   - Expression: [::]:10000/udp
	//   - Meaning: IP = <All IPv6 addresses>, Port = 10000, Protocol: UDP
	//   - It may generates multiple instances:
	//     [0] fe80::1, 10000/UDP
	//     [1] fe80::2, 10000/UDP

	// <IP>:<Port range>[/<Protocol>]
	// Split by "/" to parse IP and Port range
	auto tokens = ice_candidate.Split("/");

	if (tokens.size() > 2)
	{
		logte("Invalid ICE candidate in configuration: %s", ice_candidate.CStr());
		return false;
	}

	auto protocol = (tokens.size() == 2) ? tokens[1].UpperCaseString() : "";

	ov::SocketType type = ov::SocketType::Tcp;
	if (protocol == "UDP")
	{
		type = ov::SocketType::Udp;
	}
	else if (protocol == "TCP")
	{
		type = ov::SocketType::Tcp;
	}
	else if (protocol.IsEmpty() == false)
	{
		logte("Invalid ICE candidate found - not supported protocol: %s", tokens[1].CStr());
		return false;
	}
	else
	{
		// Use default value
	}

	const auto address = ov::SocketAddress::ParseAddress(tokens[0]);
	const auto &host = address.host;
	*port_range_list = address.port_range_list;

	if (host.IsEmpty())
	{
		// Add both of IPv4 and IPv6 addresses
		auto local_ip_list = ov::AddressUtilities::GetInstance()->GetIpList(ov::SocketFamily::Inet);
		ip_list->insert(ip_list->end(), local_ip_list.cbegin(), local_ip_list.cend());
		local_ip_list = ov::AddressUtilities::GetInstance()->GetIpList(ov::SocketFamily::Inet6);
		ip_list->insert(ip_list->end(), local_ip_list.cbegin(), local_ip_list.cend());
	}
	else if (host == "*")
	{
		// IPv4 wildcard - Add IPv4 addresses
		auto local_ip_list = ov::AddressUtilities::GetInstance()->GetIpList(ov::SocketFamily::Inet);
		ip_list->insert(ip_list->end(), local_ip_list.cbegin(), local_ip_list.cend());
	}
	else if (host == "::")
	{
		// IPv6 wildcard - Add IPv6 addresses
		auto local_ip_list = ov::AddressUtilities::GetInstance()->GetIpList(ov::SocketFamily::Inet6);
		ip_list->insert(ip_list->end(), local_ip_list.cbegin(), local_ip_list.cend());
	}
	else
	{
		ip_list->push_back(host);
	}

	*socket_type = type;

	return true;
}