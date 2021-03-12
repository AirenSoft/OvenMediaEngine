//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_port_observer.h"
#include "ice_tcp_demultiplexer.h"
#include "modules/ice/stun/stun_message.h"

#include <vector>
#include <memory>

#include <config/config.h>
#include <modules/rtp_rtcp/rtp_packet.h>
#include <modules/rtp_rtcp/rtcp_packet.h>
#include <modules/physical_port/physical_port_manager.h>


#define DEFAULT_RELAY_REALM		"airensoft"
#define DEFAULT_RELAY_USERNAME	"ome"
#define DEFAULT_RELAY_KEY		"airen"
#define DEFAULT_LIFETIME		3600
// This is the player's candidate and eventually passed to OME. 
// However, OME does not use the player's candidate. So we pass anything by this value.
#define FAKE_RELAY_IP			"1.1.1.1"
#define FAKE_RELAY_PORT			14090

class RtcIceCandidate;

class IcePort : protected PhysicalPortObserver
{
protected:
	struct GateInfo
	{
		enum class GateType
		{
			DIRECT,
			SEND_INDICATION,
			DATA_CHANNEL
		};

		IcePacketIdentifier::PacketType packet_type;
		GateType	input_method = GateType::DIRECT;
		// If this packet cames from a send 
		ov::SocketAddress peer_address;
		// If this packet is from a turn data channel, store the channel number.
		uint16_t channel_number = 0;

		ov::String ToString()
		{
			return ov::String::FormatString("Packet type : %d GateType : %d", packet_type, input_method);
		}
	};
	// A data structure to tracking client connection status
	struct IcePortInfo
	{
		std::shared_ptr<IcePortObserver> observer;

		// Session ID that connected with the client
		uint32_t session_id;
		std::any user_data;

		std::shared_ptr<const SessionDescription> offer_sdp;
		std::shared_ptr<const SessionDescription> peer_sdp;

		std::shared_ptr<ov::Socket> remote;
		ov::SocketAddress address;

		IcePortConnectionState state;

		std::chrono::time_point<std::chrono::system_clock> expire_time;

		// Information related TURN
		bool is_turn_client = false;
		bool is_data_channel_enabled = false;
		ov::SocketAddress peer_address;
		uint16_t data_channle_number = 0;

		IcePortInfo(int expire_after_ms)
			: _expire_after_ms(expire_after_ms)
		{
		}

		void UpdateBindingTime()
		{
			expire_time = std::chrono::system_clock::now() + std::chrono::milliseconds(_expire_after_ms);
		}

		bool IsExpired() const
		{
			return (std::chrono::system_clock::now() > expire_time);
		}

	protected:
		const int _expire_after_ms;
		
	};

public:
	IcePort();
	~IcePort() override;

	bool CreateTurnServer(uint16_t listening_port, ov::SocketType socket_type, int tcp_relay_worker_count);
	bool CreateIceCandidates(std::vector<RtcIceCandidate> ice_candidate_list, int ice_worker_count);
	bool Close();

	IcePortConnectionState GetState(uint32_t session_id) const
	{
		auto item = _session_table.find(session_id);
		if(item == _session_table.end())
		{
			OV_ASSERT(false, "Invalid session_id: %d", session_id);
			return IcePortConnectionState::Failed;
		}

		return item->second->state;
	}

	ov::String GenerateUfrag();

	void AddSession(const std::shared_ptr<IcePortObserver> &observer, uint32_t session_id, std::shared_ptr<const SessionDescription> offer_sdp, std::shared_ptr<const SessionDescription> peer_sdp, int stun_timeout_ms, std::any user_data);
	bool RemoveSession(uint32_t session_id);

	bool Send(uint32_t session_id, std::shared_ptr<RtpPacket> packet);
	bool Send(uint32_t session_id, std::shared_ptr<RtcpPacket> packet);
	bool Send(uint32_t session_id, const std::shared_ptr<const ov::Data> &data);

	ov::String ToString() const;

protected:
	std::shared_ptr<PhysicalPort> CreatePhysicalPort(const ov::SocketAddress &address, ov::SocketType type, int ice_worker_count);

	bool ParseIceCandidate(const ov::String &ice_candidate, std::vector<ov::String> *ip_list, ov::SocketType *socket_type, int *start_port, int *end_port);

	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
	void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;
	//--------------------------------------------------------------------

	void SetIceState(std::shared_ptr<IcePortInfo> &info, IcePortConnectionState state);

private:
	void CheckTimedoutItem();

	void OnPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, 
						GateInfo &packet_info, const std::shared_ptr<const ov::Data> &data);
	void OnStunPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, 
						GateInfo &packet_info, const std::shared_ptr<const ov::Data> &data);
	void OnChannelDataPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, 
						GateInfo &packet_info, const std::shared_ptr<const ov::Data> &data);
	void OnApplicationPacketReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, 
						GateInfo &packet_info, const std::shared_ptr<const ov::Data> &data);


	bool SendStunMessage(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, StunMessage &message, const ov::String &integity_key = "");
	bool SendStunBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const std::shared_ptr<IcePortInfo> &info);
	bool SendDataIndication(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, std::shared_ptr<ov::Data> &data);


	const std::shared_ptr<const ov::Data> CreateDataIndication(ov::SocketAddress peer_address, const std::shared_ptr<const ov::Data> &data);
	const std::shared_ptr<const ov::Data> CreateChannelDataMessage(uint16_t channel_number, const std::shared_ptr<const ov::Data> &data);

	// STUN negotiation order:
	// (State: New)
	// [Server] <-- 1. Binding Request          --- [Player]
	// (State: Checking)
	// [Server] --- 2. Binding Success Response --> [Player]
	// [Server] --- 3. Binding Request          --> [Player]
	// [Server] <-- 4. Binding Success Response --- [Player]
	// (State: Connected)
	bool ProcessStunBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessStunBindingResponse(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessTurnAllocateRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessTurnRefreshRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessTurnSendIndication(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessTurnCreatePermissionRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);
	bool ProcessTurnChannelBindRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, GateInfo &packet_info, const StunMessage &message);

	// Related TURN
	std::shared_ptr<ov::Data> _hmac_key;
	// Creating an attribute to be used in advance
	std::shared_ptr<StunAttribute>	_realm_attribute;
	std::shared_ptr<StunAttribute>	_software_attribute;
	std::shared_ptr<StunAttribute>	_nonce_attribute;
	std::shared_ptr<StunAttribute>	_xor_relayed_address_attribute;

	std::vector<std::shared_ptr<PhysicalPort>> _physical_port_list;
	std::recursive_mutex _physical_port_list_mutex;

	// Mapping table containing related information until STUN binding.
	// Once binding is complete, there is no need because it can be found by destination ip & port.
	// key: offer ufrag
	// value: IcePortInfo
	std::map<const ov::String, std::shared_ptr<IcePortInfo>> _user_mapping_table;
	std::mutex _user_mapping_table_mutex;

	// Find IcePortInfo with peer's ip:port
	// key: SocketAddress
	// value: IcePortInfo
	std::mutex _ice_port_info_mutex;
	std::map<ov::SocketAddress, std::shared_ptr<IcePortInfo>> _ice_port_info;
	// Find IcePortInfo with peer's session id
	std::map<session_id_t, std::shared_ptr<IcePortInfo>> _session_table;

	// Demultiplexer for data input through TCP
	// remote's ID : Demultiplexer
	std::shared_mutex _demultiplexers_lock;
	std::map<int, std::shared_ptr<IceTcpDemultiplexer>>	_demultiplexers;

	ov::DelayQueue _timer;
};
