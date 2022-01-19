//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "ice_port.h"
#include "ice_private.h"

#include "main/main.h"

#include "modules/ice/stun/attributes/stun_attributes.h"
#include "modules/ice/stun/stun_message.h"
#include "modules/ice/stun/channel_data_message.h"

#include <algorithm>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovcrypto/message_digest.h>
#include <config/config.h>
#include <modules/rtc_signalling/rtc_ice_candidate.h>
#include <base/info/stream.h>
#include <base/info/application.h>

IcePort::IcePort()
{
	_timer.Push(
		[this](void *paramter) -> ov::DelayQueueAction {
			CheckTimedoutItem();
			return ov::DelayQueueAction::Repeat;
		},
		1000);
	_timer.Start();
}

IcePort::~IcePort()
{
	_timer.Stop();

	Close();
}

bool IcePort::CreateIceCandidates(const std::vector<std::vector<RtcIceCandidate>> &ice_candidate_list, int ice_worker_count)
{
	std::lock_guard<std::recursive_mutex> lock_guard(_physical_port_list_mutex);

	bool succeeded = true;
	std::map<int, bool> bounded;

	for (auto &ice_candidates : ice_candidate_list)
	{
		for (auto &ice_candidate : ice_candidates)
		{
			// Find same candidate already created
			auto transport = ice_candidate.GetTransport().UpperCaseString();
			auto address = ice_candidate.GetAddress();
			ov::SocketType socket_type = (transport == "TCP") ? ov::SocketType::Tcp : ov::SocketType::Udp;

			{
				auto port = address.Port();
				auto item = bounded.find(port);

				if (item != bounded.end())
				{
					// Already opened
					continue;
				}

				bounded[port] = true;
			}

			// Bind to 0.0.0.0
			address.SetHostname(nullptr);

			// Create an ICE port using candidate information
			auto physical_port = CreatePhysicalPort(address, socket_type, ice_worker_count);
			if (physical_port == nullptr)
			{
				logte("Could not create physical port for %s/%s", address.ToString().CStr(), transport.CStr());
				succeeded = false;
				break;
			}

			logti("ICE port is bound to %s/%s (%p)", address.ToString().CStr(), transport.CStr(), physical_port.get());
			_physical_port_list.push_back(physical_port);
		}
	}

	if (succeeded)
	{

	}
	else
	{
		Close();
	}

	return succeeded;
}

bool IcePort::CreateTurnServer(const ov::SocketAddress &address, ov::SocketType socket_type, int tcp_relay_worker_count)
{
	// {[Browser][WebRTC][TURN Client]} <----(TCP)-----> {[TURN Server][OvenMediaEngine]}

	// OME introduces a built-in TURN server to support WebRTC/TCP.
	// there are networks although the network speed is high but UDP packet loss occurs very seriously. There, WebRTC/udp does not play normally. 
	
	// In order to playback in such an environment with good quality, 
	// we have built in TURN server in OME to transmit WebRTC stream to tcp. 
	// The built-in turn server does not use UDP when transmitting or receiving data from the relayed port to the peer, 
	// and only needs to copy the memory within the same process, thus the udp transmission section between OME and Player is omitted. 
	// In other words, Player and OME can communicate with only TCP.

	// If the peer is the same process as TurnServer (OME), it does not transmit through UDP and calls the function directly.
	// Player --[TURN/TCP]--> [TurnServer(OME) --[Fucntion Call not udp send]--> Peer(OME)]
	// Player <--[TURN/TCP]-- [TurnServer(OME) <--[Fucntion Call not udp send]-- Peer(OME)]

	// ov::SocketAddress address(listening_port);
	auto physical_port = CreatePhysicalPort(address, socket_type, tcp_relay_worker_count);
	if (physical_port == nullptr)
	{
		logte("Could not create physical port for %s/%s", address.ToString().CStr(), StringFromSocketType(socket_type));
		return false;
	}

	logti("ICE port is bound to %s/%s (%p)", address.ToString().CStr(), StringFromSocketType(socket_type), physical_port.get());
	_physical_port_list.push_back(physical_port);

	// make HMAC key
	// https://tools.ietf.org/html/rfc8489#section-9.2.2
	//  key = MD5(username ":" OpaqueString(realm) ":" OpaqueString(password))
	_hmac_key = ov::MessageDigest::ComputeDigest(ov::CryptoAlgorithm::Md5, 
		ov::String::FormatString("%s:%s:%s", DEFAULT_RELAY_USERNAME, DEFAULT_RELAY_REALM, DEFAULT_RELAY_KEY).ToData(false));

	// Creating an attribute to be used in advance

	// Nonce
	auto nonce_attribute = std::make_shared<StunNonceAttribute>();
	nonce_attribute->SetText("1bcf94ca7494141e");
	_nonce_attribute = std::move(nonce_attribute);

	// Realm
	auto realm_attribute = std::make_shared<StunRealmAttribute>();
	realm_attribute->SetText(DEFAULT_RELAY_REALM);
	_realm_attribute = std::move(realm_attribute);

	// Software
	auto software_attribute = std::make_shared<StunSoftwareAttribute>();
	software_attribute->SetText(ov::String::FormatString("OvenMediaEngine v%s", OME_VERSION));
	_software_attribute = std::move(software_attribute);

	// Add XOR-RELAYED-ADDRESS attribute
	// This is the player's candidate and passed to OME.
	// However, OME does not use the player's candidate. So we pass anything by this value.
	auto xor_relayed_address_attribute = std::make_shared<StunXorRelayedAddressAttribute>();
	xor_relayed_address_attribute->SetParameters(ov::SocketAddress(FAKE_RELAY_IP, FAKE_RELAY_PORT));
	_xor_relayed_address_attribute = std::move(xor_relayed_address_attribute);

	return true;
}

std::shared_ptr<PhysicalPort> IcePort::CreatePhysicalPort(const ov::SocketAddress &address, ov::SocketType type, int worker_count)
{
	auto physical_port = PhysicalPortManager::GetInstance()->CreatePort("ICE", type, address, worker_count);
	if (physical_port != nullptr)
	{
		if (physical_port->AddObserver(this))
		{
			return physical_port;
		}

		logte("Cannot add a observer %p to %p", this, physical_port.get());

		PhysicalPortManager::GetInstance()->DeletePort(physical_port);
	}
	else
	{
		logte("Cannot create physical port for %s (type: %s), workers: %d", address.ToString().CStr(), ov::StringFromSocketType(type), worker_count);
	}

	return nullptr;
}

bool IcePort::Close()
{
	std::lock_guard<std::recursive_mutex> lock_guard(_physical_port_list_mutex);

	bool result = true;

	for (auto &physical_port : _physical_port_list)
	{
		result = result && physical_port->RemoveObserver(this);
		result = result && PhysicalPortManager::GetInstance()->DeletePort(physical_port);

		if (result == false)
		{
			logtd("Cannot close ICE port");
			break;
		}
	}

	_timer.Stop();

	return result;
}

ov::String IcePort::GenerateUfrag()
{
	std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);

	while (true)
	{
		ov::String ufrag = ov::Random::GenerateString(6);

		if (_user_port_table.find(ufrag) == _user_port_table.end())
		{
			logtd("Generated ufrag: %s", ufrag.CStr());

			return ufrag;
		}
	}
}

void IcePort::AddSession(const std::shared_ptr<IcePortObserver> &observer, uint32_t session_id, 
							std::shared_ptr<const SessionDescription> offer_sdp, std::shared_ptr<const SessionDescription> peer_sdp, 
							int expired_ms, uint64_t life_time_epoch_ms, std::any user_data)
{
	const ov::String &local_ufrag = offer_sdp->GetIceUfrag();
	const ov::String &remote_ufrag = peer_sdp->GetIceUfrag();

	{
		std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);

		auto item = _user_port_table.find(local_ufrag);
		if (item != _user_port_table.end())
		{
			OV_ASSERT(false, "Duplicated ufrag: %s:%s, session_id: %d (old session_id: %d)", local_ufrag.CStr(), remote_ufrag.CStr(), session_id, item->second->session_id);
		}

		logtd("Trying to add session: %d (ufrag: %s:%s)...", session_id, local_ufrag.CStr(), remote_ufrag.CStr());

		std::shared_ptr<IcePortInfo> info = std::make_shared<IcePortInfo>(expired_ms, life_time_epoch_ms);

		info->observer = observer;
		info->user_data = user_data;
		info->session_id = session_id;
		info->offer_sdp = offer_sdp;
		info->peer_sdp = peer_sdp;
		info->remote = nullptr;
		info->address = ov::SocketAddress();
		info->state = IcePortConnectionState::Closed;

		info->UpdateBindingTime();

		_user_port_table[local_ufrag] = info;
	}

	SetIceState(_user_port_table[local_ufrag], IcePortConnectionState::New);
}

bool IcePort::RemoveSession(uint32_t session_id)
{
	std::shared_ptr<IcePortInfo> ice_port_info;

	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);

		auto item = _session_port_table.find(session_id);
		if (item == _session_port_table.end())
		{
			/*
			The case of reaching here is as follows.

			1. Already the session was deleted but WebRTC Signalling server try to delete the session again
			2. IcePort sent Stun request but player didn't response stun bind response

			*/
			logtd("Could not find session: %d", session_id);

			{
				// If it exists only in _user_port_table, find it and remove it.
				// TODO(Dimiden): In this case, apply a more efficient method of deletion.
				std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);

				auto it = _user_port_table.begin();
				while(it != _user_port_table.end())
				{
					auto ice_port_info = it->second;
					if (ice_port_info->session_id == session_id)
					{
						_user_port_table.erase(it++);
						logtd("This is because the stun request was not received from this session.");

						// Close only TCP (TURN)
						auto remote = ice_port_info->remote;

						if (remote != nullptr)
						{
							if (remote->GetSocket().GetType() == ov::SocketType::Tcp)
							{
								remote->CloseIfNeeded();
							}
						}

						return true;
					}
					else
					{
						it++;
					}
				}
			}

			return false;
		}

		ice_port_info = item->second;

		_session_port_table.erase(item);
		for(const auto &item : ice_port_info->address_map)
		{
			_address_port_table.erase(item.first);
		}

		// Close only TCP (TURN)
		auto remote = ice_port_info->remote;

		if (remote != nullptr)
		{
			if (remote->GetSocket().GetType() == ov::SocketType::Tcp)
			{
				remote->CloseIfNeeded();
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);
		_user_port_table.erase(ice_port_info->offer_sdp->GetIceUfrag());
	}

	return true;
}

void IcePort::CheckTimedoutItem()
{
	// Remove expired transction items
	{
		std::lock_guard<std::shared_mutex> brt_lock(_binding_request_table_lock);

		for(auto it = _binding_request_table.begin(); it != _binding_request_table.end();)
		{
			if(it->second.IsExpired())
			{
				it = _binding_request_table.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	std::vector<std::shared_ptr<IcePortInfo>> delete_list;
	{
		std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);

		for (auto item = _user_port_table.begin(); item != _user_port_table.end();)
		{
			if (item->second->IsExpired())
			{
				delete_list.push_back(item->second);
				item = _user_port_table.erase(item);
			}
			else
			{
				++item;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);

		for (auto &deleted_ice_port : delete_list)
		{
			_session_port_table.erase(deleted_ice_port->session_id);
			for(const auto &item : deleted_ice_port->address_map)
			{
				_address_port_table.erase(item.first);
			}
		}
	}

	// Notify to observer
	for (auto &deleted_ice_port : delete_list)
	{
		logtw("Client %s(session id: %d) has expired", deleted_ice_port->address.ToString(false).CStr(), deleted_ice_port->session_id);

		// Close only TCP (TURN)
		if(deleted_ice_port->remote != nullptr && deleted_ice_port->remote->GetSocket().GetType() == ov::SocketType::Tcp)
		{
			deleted_ice_port->remote->CloseIfNeeded();
		}

		SetIceState(deleted_ice_port, IcePortConnectionState::Disconnected);
	}
}

bool IcePort::Send(uint32_t session_id, std::shared_ptr<RtpPacket> packet)
{
	return Send(session_id, packet->GetData());
}

bool IcePort::Send(uint32_t session_id, std::shared_ptr<RtcpPacket> packet)
{
	return Send(session_id, packet->GetData());
}

bool IcePort::Send(uint32_t session_id, const std::shared_ptr<const ov::Data> &data)
{
	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);

		auto item = _session_port_table.find(session_id);
		if (item == _session_port_table.end())
		{
			logtd("ClientSocket not found for session #%d", session_id);
			return false;
		}

		ice_port_info = item->second;
	}

	std::shared_ptr<const ov::Data> send_data = nullptr;

	// Send throutgh TURN data channel
	if(ice_port_info->is_turn_client == true && ice_port_info->is_data_channel_enabled == true)
	{	
		send_data = CreateChannelDataMessage(ice_port_info->data_channle_number, data);
	}
	// Send thourgh DATA indication
	else if(ice_port_info->is_turn_client == true && ice_port_info->is_data_channel_enabled == false)
	{
		send_data = CreateDataIndication(ice_port_info->peer_address, data);
	}
	// Send direct
	else
	{
		send_data = data;
	}

	if(send_data == nullptr)
	{
		return false;
	}

	auto remote = ice_port_info->remote;

	if (remote == nullptr)
	{
		return false;
	}

	return remote->SendTo(ice_port_info->address, send_data);
}

void IcePort::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	// called when TURN client connected to the turn server with TCP
	auto demultiplexer = std::make_shared<IceTcpDemultiplexer>();

	std::lock_guard<std::shared_mutex> lock_guard(_demultiplexers_lock);
	_demultiplexers[remote->GetNativeHandle()] = demultiplexer;

	logti("Turn client has connected : %s", remote->ToString().CStr());
}

void IcePort::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	// called when TURN client disconnected from the turn server with TCP
	std::lock_guard<std::shared_mutex> lock_guard(_demultiplexers_lock);

	auto it = _demultiplexers.find(remote->GetNativeHandle());
	if(it != _demultiplexers.end())
	{
		_demultiplexers.erase(remote->GetNativeHandle());
	}

	logti("Turn client has disconnected : %s", remote->ToString().CStr());
}

void IcePort::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	// The only packet input to IcePort/TCP is STUN and TURN DATA CHANNEL.
	if(remote->GetType() == ov::SocketType::Tcp)
	{
		std::shared_lock<std::shared_mutex> lock(_demultiplexers_lock);
		// If remote protocol is tcp, it must be TURN
		if(_demultiplexers.find(remote->GetNativeHandle()) == _demultiplexers.end())
		{
			// If the client disconnects at this time, it cannot be found.
			logtd("TCP packet input but cannot find the demultiplexer of %s.", remote->ToString().CStr());
			return;
		}

		auto demultiplexer = _demultiplexers[remote->GetNativeHandle()];
		lock.unlock();

		// TCP demultiplexer 
		demultiplexer->AppendData(data);

		while(demultiplexer->IsAvailablePacket())
		{
			auto packet = demultiplexer->PopPacket();
		
			GateInfo gate_info;
			gate_info.packet_type = packet->GetPacketType();
			OnPacketReceived(remote, address, gate_info, packet->GetData());
		}
	}
	else if(remote->GetType() == ov::SocketType::Udp)
	{
		GateInfo gate_info;
		gate_info.packet_type = IcePacketIdentifier::FindPacketType(data);

		OnPacketReceived(remote, address, gate_info, data);
	}
}

void IcePort::OnPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const std::shared_ptr<const ov::Data> &data)
{
	logtd("OnPacketReceived : %s", gate_info.ToString().CStr());

	switch(gate_info.packet_type)
	{
		case IcePacketIdentifier::PacketType::TURN_CHANNEL_DATA:
			OnChannelDataPacketReceived(remote, address, gate_info, data);
			break;
		case IcePacketIdentifier::PacketType::STUN:
			OnStunPacketReceived(remote, address, gate_info, data);
			break;
		case IcePacketIdentifier::PacketType::RTP_RTCP:
		case IcePacketIdentifier::PacketType::DTLS:
			OnApplicationPacketReceived(remote, address, gate_info, data);
			break;
		case IcePacketIdentifier::PacketType::ZRTP:
		case IcePacketIdentifier::PacketType::UNKNOWN:
			break;
	}
}

void IcePort::OnApplicationPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, 
						GateInfo &gate_info, const std::shared_ptr<const ov::Data> &data)
{
	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);
		auto item = _address_port_table.find(address);
		if (item != _address_port_table.end())
		{
			ice_port_info = item->second;
			// When the candidate pair is determined, the peer starts sending DTLS messages. This can be seen as a true connected.
			if(ice_port_info->state != IcePortConnectionState::Connected)
			{
				SetIceState(ice_port_info, IcePortConnectionState::Connected);
				// It communicates with the candidate address that sends application data first.
				ice_port_info->address = address;
			}
		}
	}

	if (ice_port_info == nullptr)
	{
		logtw("Could not find client(%s) information. Dropping...", address.ToString(false).CStr());
		return;
	}
	
	if(ice_port_info->observer != nullptr)
	{
		ice_port_info->observer->OnDataReceived(*this, ice_port_info->session_id, data, ice_port_info->user_data);
	}
}

void IcePort::OnChannelDataPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const std::shared_ptr<const ov::Data> &data)
{
	ChannelDataMessage message;

	if(message.Load(data) == false)
	{
		return;
	}

	GateInfo application_gate_info;

	application_gate_info.input_method = IcePort::GateInfo::GateType::DATA_CHANNEL;
	application_gate_info.channel_number = message.GetChannelNumber();
	application_gate_info.packet_type = IcePacketIdentifier::FindPacketType(message.GetData());

	// Update GateInfo
	// If a request comes from a send indication or channel, this is through a turn. When transmitting a packet to the player, it must be sent through a data indication or channel, so it stores related information.
	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);
		auto item = _address_port_table.find(address);
		if (item != _address_port_table.end())
		{
			ice_port_info = item->second;
			ice_port_info->is_turn_client = true;
			ice_port_info->is_data_channel_enabled = true;
			ice_port_info->data_channle_number = application_gate_info.channel_number;
		}
	}

	// Decapsulate and process the packet again.
	OnPacketReceived(remote, address, application_gate_info, message.GetData());
}

void IcePort::OnStunPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const std::shared_ptr<const ov::Data> &data)
{
	ov::ByteStream stream(data.get());
	StunMessage message;

	if (message.Parse(stream) == false)
	{
		logte("Could not parse STUN packet from %s", remote->ToString().CStr());
		return;
	}

	logtd("Received message:\n%s", message.ToString().CStr());

	if(message.GetClass() == StunClass::ErrorResponse)
	{
		// Print
		auto error_code = message.GetAttribute<StunErrorCodeAttribute>(StunAttributeType::ErrorCode);
		if(error_code == nullptr)
		{
			logtw("Received stun error response, but there is no ErrorCode attribute");
		}
		else
		{
			logtw("Received stun error response (Error code : %d Reason : %s)", error_code->GetErrorCodeNumber(), error_code->GetErrorReason().CStr());
		}
	}

	switch(message.GetMethod())
	{
		// STUN
		case StunMethod::Binding:
		{
			switch(message.GetClass())
			{
				case StunClass::Request:
				case StunClass::Indication:
					ProcessStunBindingRequest(remote, address, gate_info, message);
					break;
				case StunClass::SuccessResponse:
					ProcessStunBindingResponse(remote, address, gate_info, message);
					break;
				case StunClass::ErrorResponse:
					//TODO(Getroot): Delete the transaction immediately. 
					break;
			}
			break;
		}
		// TURN Server
		// Because it is a turn server, no response class comes.
		case StunMethod::Allocate:
			if(message.GetClass() == StunClass::Request)
			{
				ProcessTurnAllocateRequest(remote, address, gate_info, message);
			}
			break;
		case StunMethod::Refresh:
			if(message.GetClass() == StunClass::Request)
			{
				ProcessTurnRefreshRequest(remote, address, gate_info, message);
			}
			break;
		case StunMethod::Send:
			if(message.GetClass() == StunClass::Indication)
			{
				ProcessTurnSendIndication(remote, address, gate_info, message);
			}
			break;
		case StunMethod::CreatePermission:
			if(message.GetClass() == StunClass::Request)
			{
				ProcessTurnCreatePermissionRequest(remote, address, gate_info, message);
			}
			break;
		case StunMethod::ChannelBind:
			if(message.GetClass() == StunClass::Request)
			{
				ProcessTurnChannelBindRequest(remote, address, gate_info, message);
			}
			break;
		case StunMethod::Data:
			// Since this is a turn server, it does not receive a data method.
			logtd("Bad Packet - TURN Server cannot receive the Stun Data method(%s)", remote->ToString().CStr());
			break;
		default:
			OV_ASSERT(false, "Not implemented method: %d", message.GetMethod());
			logtw("Unknown method: %d", message.GetMethod());
			break;
	}
}

bool IcePort::ProcessStunBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	// Binding Request
	ov::String local_ufrag;
	ov::String remote_ufrag;

	if (message.GetUfrags(&local_ufrag, &remote_ufrag) == false)
	{
		logtw("Could not process user name attribute");
		return false;
	}

	logtd("[From %s To %s] Received STUN binding request: %s:%s", address.ToString(false).CStr(), remote->GetLocalAddress()->ToString().CStr(), local_ufrag.CStr(), remote_ufrag.CStr());

	
	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		// WebRTC Publisher registers ufrag with session information 
		// through IcePort::AddSession function after signaling with player
		std::unique_lock<std::mutex> lock_guard(_user_port_table_lock);
		auto info = _user_port_table.find(local_ufrag);
		if (info == _user_port_table.end())
		{
			// Stun may arrive first before AddSession, it is not an error
			logtd("User not found: %s (AddSession() needed)", local_ufrag.CStr());
			return false;
		}

		ice_port_info = info->second;
	}

	if (ice_port_info->peer_sdp->GetIceUfrag() != remote_ufrag)
	{
		logtw("Mismatched ufrag: %s (ufrag in peer SDP: %s)", remote_ufrag.CStr(), ice_port_info->peer_sdp->GetIceUfrag().CStr());
	}

	if (message.CheckIntegrity(ice_port_info->offer_sdp->GetIcePwd()) == false)
	{
		logtw("Failed to check integrity");

		SetIceState(ice_port_info, IcePortConnectionState::Failed);
		{
			std::lock_guard<std::mutex> lock_guard(_user_port_table_lock);

			_user_port_table.erase(local_ufrag);
		}

		{
			std::lock_guard<std::mutex> lock_guard(_port_table_lock);

			for(const auto &item : ice_port_info->address_map)
			{
				_address_port_table.erase(item.first);
			}
			_session_port_table.erase(ice_port_info->session_id);
		}

		return false;
	}

	if (ice_port_info->state == IcePortConnectionState::New || 
		(ice_port_info->state == IcePortConnectionState::Checking && ice_port_info->address != address) )
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);

		if(ice_port_info->state == IcePortConnectionState::New)
		{
			logti("Add the client to the port list: %s / %s", address.ToString(false).CStr(), gate_info.ToString().CStr());
		}
		else
		{
			logti("Update the client to the port list: to %s from %s", address.ToString(false).CStr(), ice_port_info->address.ToString(false).CStr());
		}

		ice_port_info->remote = remote;
		ice_port_info->address = address;
		ice_port_info->address_map[address] = true;

		_address_port_table[address] = ice_port_info;
		_session_port_table[ice_port_info->session_id] = ice_port_info;

		SetIceState(ice_port_info, IcePortConnectionState::Checking);
	}

	ice_port_info->UpdateBindingTime();

	// If the class is Indication it doesn't need to send response
	if(message.GetClass() == StunClass::Request)
	{
		StunMessage response_message;
		response_message.SetHeader(StunClass::SuccessResponse, StunMethod::Binding, message.GetTransactionId());

		// Add XOR-MAPPED-ADDRESS attribute
		// If client is relay, then use relay IP/port
		auto xor_mapped_attribute = std::make_shared<StunXorMappedAddressAttribute>();
		
		if(gate_info.input_method == GateInfo::GateType::DIRECT)
		{
			xor_mapped_attribute->SetParameters(address);
		}
		else
		{
			xor_mapped_attribute->SetParameters(ov::SocketAddress(FAKE_RELAY_IP, FAKE_RELAY_PORT));
		}

		response_message.AddAttribute(std::move(xor_mapped_attribute));

		// Send Stun Binding Response
		// TODO: apply SASLprep(password)
		SendStunMessage(remote, address, gate_info, response_message, ice_port_info->offer_sdp->GetIcePwd().ToData(false));

		// Immediately, the server also sends a bind request.
		SendStunBindingRequest(remote, address, gate_info, ice_port_info);
	}

	return true; 
}

bool IcePort::SendStunBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const std::shared_ptr<IcePortInfo> &info)
{
	StunMessage message;

	message.SetClass(StunClass::Request);
	message.SetMethod(StunMethod::Binding);
	// TODO: make transaction_id unique
	uint8_t transaction_id[OV_STUN_TRANSACTION_ID_LENGTH];
	uint8_t charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	// generate transaction id ramdomly
	for (int index = 0; index < OV_STUN_TRANSACTION_ID_LENGTH; index++)
	{
		transaction_id[index] = charset[rand() % (OV_COUNTOF(charset)-1)];
	}
	message.SetTransactionId(&(transaction_id[0]));

	std::shared_ptr<StunAttribute> attribute;

	// USERNAME attribute 
	attribute = std::make_shared<StunUserNameAttribute>();
	auto *user_name_attribute = dynamic_cast<StunUserNameAttribute *>(attribute.get());
	user_name_attribute->SetText(ov::String::FormatString("%s:%s", info->peer_sdp->GetIceUfrag().CStr(), info->offer_sdp->GetIceUfrag().CStr()));
	message.AddAttribute(std::move(attribute));

	// ICE-CONTROLLED
	//attribute = std::make_shared<StunUnknownAttribute>(0x8029, 8);
	// ICE-CONTROLLING 
	attribute = std::make_shared<StunUnknownAttribute>(0x802A, 8);
	auto *tie_break_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());

	/*
	https://datatracker.ietf.org/doc/html/rfc5245#section-7.2.1.1
	If the agent's tie-breaker is less than the contents of the
    ICE-CONTROLLING attribute, the agent switches to the controlled role.
	*/
	//uint8_t tie_break_value[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t tie_break_value[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
	
	tie_break_attribute->SetData(&(tie_break_value[0]), 8);
	message.AddAttribute(std::move(attribute));

	// USE-CANDIDATE (for testing hash)
	StunUnknownAttribute *unknown_attribute = nullptr;
	attribute = std::make_shared<StunUnknownAttribute>(0x0025, 0);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	message.AddAttribute(std::move(attribute));

	// PRIORITY (for testing hash)
	attribute = std::make_shared<StunUnknownAttribute>(0x0024, 4);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data3[] = {0x6E, 0x7F, 0x1E, 0xFF};
	unknown_attribute->SetData(&(unknown_data3[0]), 4);
	message.AddAttribute(std::move(attribute));

	logtd("Send Stun Binding Request : %s", address.ToString(false).CStr());

	// Store binding request transction
	{
		std::lock_guard<std::shared_mutex> brt_lock(_binding_request_table_lock);

		ov::String transaction_id_key((char*)(&transaction_id[0]), OV_STUN_TRANSACTION_ID_LENGTH);
		_binding_request_table.emplace(transaction_id_key, BindingRequestInfo(transaction_id_key, info));

		logtd("Send Binding Request to(%s) id(%s)", address.ToString(false).CStr(), transaction_id_key.CStr());
	}

	// TODO: apply SASLprep(password)
	SendStunMessage(remote, address, gate_info, message, info->peer_sdp->GetIcePwd().ToData(false));

	return true;
}

bool IcePort::ProcessStunBindingResponse(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		// Find reqeusted info in the table
		std::lock_guard<std::shared_mutex> brt_lock(_binding_request_table_lock);

		ov::String transaction_id_key((char*)(&message.GetTransactionId()[0]), OV_STUN_TRANSACTION_ID_LENGTH);

		auto item = _binding_request_table.find(transaction_id_key);
		if(item == _binding_request_table.end())
		{
			logtw("Could not find binding request info : address(%s) transaction id(%s)", address.ToString(false).CStr(), transaction_id_key.CStr());
			return false;
		}

		ice_port_info = item->second._ice_port;

		// Erase ended transction item
		_binding_request_table.erase(item);

		logtd("Receive stun binding response from %s, table size(%d)", address.ToString(false).CStr(), _binding_request_table.size());
	}

	if (message.CheckIntegrity(ice_port_info->offer_sdp->GetIcePwd()) == false)
	{
		logtw("Failed to check integrity");
		return false;
	}

	logtd("Client %s sent STUN binding response", address.ToString(false).CStr());

	return true;
}

bool IcePort::SendStunMessage(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info,  StunMessage &message, const std::shared_ptr<const ov::Data> &integrity_key)
{
	std::shared_ptr<const ov::Data> source_data, send_data;

	if(integrity_key == nullptr)
	{
		source_data = message.Serialize();
	}
	else
	{
		source_data = message.Serialize(integrity_key);
	}
	
	logtd("Send message:\n%s", message.ToString().CStr());

	if(gate_info.input_method == IcePort::GateInfo::GateType::DIRECT)
	{
		send_data = source_data;
	}
	else if(gate_info.input_method == IcePort::GateInfo::GateType::SEND_INDICATION)
	{
		send_data = CreateDataIndication(gate_info.peer_address, source_data);
	}
	else if(gate_info.input_method == IcePort::GateInfo::GateType::DATA_CHANNEL)
	{
		send_data = CreateChannelDataMessage(gate_info.channel_number, source_data);
	}

	if(send_data == nullptr)
	{
		return false;
	}

	auto sent_bytes = remote->SendTo(address, send_data);

	return sent_bytes > 0;
}

const std::shared_ptr<const ov::Data> IcePort::CreateDataIndication(ov::SocketAddress peer_address, const std::shared_ptr<const ov::Data> &data)
{
	StunMessage send_indication_message;
	send_indication_message.SetHeader(StunClass::Indication, StunMethod::Data, reinterpret_cast<uint8_t *>(ov::Random::GenerateString(20).GetBuffer()));

	auto data_attribute = std::make_shared<StunDataAttribute>();
	data_attribute->SetData(data);
	send_indication_message.AddAttribute(std::move(data_attribute));

	auto xor_peer_attribute = std::make_shared<StunXorPeerAddressAttribute>();
	xor_peer_attribute->SetParameters(peer_address);
	send_indication_message.AddAttribute(std::move(xor_peer_attribute));

	send_indication_message.AddAttribute(_software_attribute);

	auto send_data = send_indication_message.Serialize();

	logtd("Send Data Indication:\n%s", send_indication_message.ToString().CStr());

	return send_data;
}

const std::shared_ptr<const ov::Data> IcePort::CreateChannelDataMessage(uint16_t channel_number, const std::shared_ptr<const ov::Data> &data)
{
	ChannelDataMessage	channel_data_message(channel_number, data);
	return channel_data_message.GetPacket();
}


bool IcePort::ProcessTurnAllocateRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	StunMessage response_message;

	auto requested_transport_attr = message.GetAttribute<StunRequestedTransportAttribute>(StunAttributeType::RequestedTransport);
	if(requested_transport_attr == nullptr)
	{
		response_message.SetHeader(StunClass::ErrorResponse, StunMethod::Allocate, message.GetTransactionId());
		response_message.SetErrorCodeAttribute(StunErrorCode::BadRequest, "REQUESTED-TRANSPORT attribute is not included");
		SendStunMessage(remote, address, gate_info, response_message);
		return false;
	}

	// only protocol number 17(UDP) is allowed (https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml)
	if(requested_transport_attr->GetProtocolNumber() != 17)
	{
		response_message.SetHeader(StunClass::ErrorResponse, StunMethod::Allocate, message.GetTransactionId());
		response_message.SetErrorCodeAttribute(StunErrorCode::UnsupportedTransportProtocol);
		SendStunMessage(remote, address, gate_info, response_message);
		return false;
	}

	auto integrity_attribute = message.GetAttribute<StunMessageIntegrityAttribute>(StunAttributeType::MessageIntegrity);
	if(integrity_attribute == nullptr)
	{
		// First request
		response_message.SetHeader(StunClass::ErrorResponse, StunMethod::Allocate, message.GetTransactionId());
		response_message.SetErrorCodeAttribute(StunErrorCode::Unauthonticated);

		response_message.AddAttribute(_nonce_attribute);
		response_message.AddAttribute(_realm_attribute);
		response_message.AddAttribute(_software_attribute);

		SendStunMessage(remote, address, gate_info, response_message);

		// This is the original protocol specification.
		return true;
	}

	// TODO: Check authentication information, USERNAME, REALM, NONCE, MESSAGE-INTEGRITY

	response_message.SetHeader(StunClass::SuccessResponse, StunMethod::Allocate, message.GetTransactionId());

	// Add XOR-MAPPED-ADDRESS attribute
	auto xor_mapped_address_attribute = std::make_shared<StunXorMappedAddressAttribute>();
	xor_mapped_address_attribute->SetParameters(address);
	response_message.AddAttribute(std::move(xor_mapped_address_attribute));

	// Add lifetime
	uint32_t lifetime = DEFAULT_LIFETIME;
	auto requested_lifetime_attribute = message.GetAttribute<StunLifetimeAttribute>(StunAttributeType::Lifetime);
	if(requested_lifetime_attribute != nullptr)
	{
		lifetime = std::min(static_cast<uint32_t>(DEFAULT_LIFETIME), requested_lifetime_attribute->GetValue());
	}

	auto lifetime_attribute = std::make_shared<StunLifetimeAttribute>();
	lifetime_attribute->SetValue(lifetime);
	response_message.AddAttribute(lifetime_attribute);

	response_message.AddAttribute(_xor_relayed_address_attribute);
	response_message.AddAttribute(_software_attribute);
	
	SendStunMessage(remote, address, gate_info, response_message, _hmac_key);

	return true;
}

bool IcePort::ProcessTurnSendIndication(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	auto xor_peer_attribute = message.GetAttribute<StunXorPeerAddressAttribute>(StunAttributeType::XorPeerAddress);
	if(xor_peer_attribute == nullptr)
	{
		return false;
	}
	
	auto data_attribute = message.GetAttribute<StunDataAttribute>(StunAttributeType::Data);
	if(data_attribute == nullptr)
	{
		return false;
	}

	auto data = data_attribute->GetData();

	gate_info.packet_type = IcePacketIdentifier::FindPacketType(data);
	gate_info.input_method = IcePort::GateInfo::GateType::SEND_INDICATION;
	gate_info.peer_address = xor_peer_attribute->GetAddress();

	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);
		auto item = _address_port_table.find(address);
		if (item != _address_port_table.end())
		{
			ice_port_info = item->second;
			ice_port_info->is_turn_client = true;
			ice_port_info->is_data_channel_enabled = false;
			ice_port_info->peer_address = gate_info.peer_address;
		}
	}

	OnPacketReceived(remote, address, gate_info, data);

	return true;
}

bool IcePort::ProcessTurnCreatePermissionRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	//TODO(Getroot): Check validation

	StunMessage response_message;
	response_message.SetHeader(StunClass::SuccessResponse, StunMethod::CreatePermission, message.GetTransactionId());
	SendStunMessage(remote, address, gate_info, response_message, _hmac_key);

	return true;
}

bool IcePort::ProcessTurnChannelBindRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	StunMessage response_message;

	auto channel_number_attribute = message.GetAttribute<StunChannelNumberAttribute>(StunAttributeType::ChannelNumber);
	if(channel_number_attribute == nullptr)
	{
		response_message.SetHeader(StunClass::ErrorResponse, StunMethod::ChannelBind, message.GetTransactionId());
		response_message.SetErrorCodeAttribute(StunErrorCode::BadRequest);
		SendStunMessage(remote, address, gate_info, response_message);
		return false;
	}

	response_message.SetHeader(StunClass::SuccessResponse, StunMethod::ChannelBind, message.GetTransactionId());
	SendStunMessage(remote, address, gate_info, response_message, _hmac_key);

	std::shared_ptr<IcePortInfo> ice_port_info;
	{
		std::lock_guard<std::mutex> lock_guard(_port_table_lock);
		auto item = _address_port_table.find(address);
		if (item != _address_port_table.end())
		{
			ice_port_info = item->second;
			ice_port_info->is_turn_client = true;
			ice_port_info->is_data_channel_enabled = true;
			ice_port_info->data_channle_number = channel_number_attribute->GetChannelNumber();
		}
	}

	return true;
}

bool IcePort::ProcessTurnRefreshRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &gate_info, const StunMessage &message)
{
	StunMessage response_message;

	// Add lifetime
	uint32_t lifetime = DEFAULT_LIFETIME;

	auto requested_lifetime_attr = message.GetAttribute<StunLifetimeAttribute>(StunAttributeType::Lifetime);
	if(requested_lifetime_attr != nullptr)
	{
		lifetime = std::min(static_cast<uint32_t>(DEFAULT_LIFETIME), requested_lifetime_attr->GetValue());
	}

	auto lifetime_attribute = std::make_shared<StunLifetimeAttribute>();
	lifetime_attribute->SetValue(lifetime);
	response_message.AddAttribute(lifetime_attribute);

	response_message.SetHeader(StunClass::SuccessResponse, StunMethod::Refresh, message.GetTransactionId());
	response_message.AddAttribute(lifetime_attribute);
	SendStunMessage(remote, address, gate_info, response_message, _hmac_key);

	logtd("Turn Refresh Request : %s", lifetime_attribute->ToString().CStr());	

	return true;
}

void IcePort::SetIceState(std::shared_ptr<IcePortInfo> &info, IcePortConnectionState state)
{
	info->state = state;
	if(info->observer != nullptr)
	{
		info->observer->OnStateChanged(*this, info->session_id, state, info->user_data);
	}
}

ov::String IcePort::ToString() const
{
	return ov::String::FormatString("<IcePort: %p, %zu ports>", this, _physical_port_list.size());
}
