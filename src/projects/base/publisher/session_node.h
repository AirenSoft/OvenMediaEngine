//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include "session.h"

namespace pub
{
	// Session에서 사용하는 Protocol Stack을 정의한다.
	// 이 값을 이용하여 NODE ID로도 활용할 수 있도록 각 값의 범위를 많이 벌려 놓는다.
	// RTP 같은 경우는 SSRC 와 같이 Packet의 값과 일치하는 값을 사용하면 쉽게 해당 RTP 노드를 찾아서 패킷을 전달 할 수 있다.
	// ID 값은 연결되는 Node 간에만 Unique하면 되므로 큰 값은 필요치 않다.
	enum class SessionNodeType : int16_t
	{
		None = 0,
		Rtp = 100,
		Rtcp = 101,
		Srtp = 200,
		Sctp = 300,
		Dtls = 400,
		Ice = 500
	};

	class SessionNode : public ov::EnableSharedFromThis<SessionNode>
	{
	public:
		SessionNode(uint32_t id, SessionNodeType node_type, std::shared_ptr<Session> session);
		virtual ~SessionNode();

		uint32_t GetId();
		SessionNodeType GetNodeType();

		virtual void RegisterUpperNode(std::shared_ptr<SessionNode> node);
		virtual void RegisterLowerNode(std::shared_ptr<SessionNode> node);

		virtual bool Start();
		virtual bool Stop();

		enum class NodeState : int8_t
		{
			Ready,
			Started,
			Stopped,
			Error
		};

		NodeState GetState();

		// 데이터를 upper에서 받는다. lower node로 보낸다.
		virtual bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) = 0;
		// 데이터를 lower에서 받는다. upper node로 보낸다.
		virtual bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) = 0;

	protected:
		std::shared_ptr<Session> GetSession();

		// 하나만 연결되어 있을 때 사용한다.
		std::shared_ptr<SessionNode> GetUpperNode();
		std::shared_ptr<SessionNode> GetLowerNode();

		// 특수한 경우 여러개의 Node를 연결하여 선택적으로 보낼 수 있다.
		std::shared_ptr<SessionNode> GetUpperNode(SessionNodeType node_type);
		std::shared_ptr<SessionNode> GetLowerNode(SessionNodeType node_type);

		// 직접 접근하여 검색을 해야 할 경우 사용한다.
		std::map<SessionNodeType, std::shared_ptr<SessionNode>> _upper_nodes;
		std::map<SessionNodeType, std::shared_ptr<SessionNode>> _lower_nodes;

	private:
		SessionNodeType _node_type;
		uint32_t _node_id;
		std::shared_ptr<Session> _session;
		NodeState _state;
	};
}  // namespace pub