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

typedef uint32_t node_id_t;

namespace ov
{
	class Node : public ov::EnableSharedFromThis<Node>
	{
	public:
		Node(NodeType node_type);
		virtual ~Node();

		NodeType GetNodeType();

		virtual void RegisterUpperNode(const std::shared_ptr<Node> &node);
		virtual void RegisterLowerNode(const std::shared_ptr<Node> &node);

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

		// It receives data from the upper side. Send it to the lower node.
		virtual bool SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data) = 0;
		// Receive data from lower. Send to the upper node.
		virtual bool OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data) = 0;

	protected:
		std::shared_ptr<Node> GetUpperNode();
		std::shared_ptr<Node> GetLowerNode();

		std::shared_ptr<Node> GetUpperNode(NodeType node_type);
		std::shared_ptr<Node> GetLowerNode(NodeType node_type);

		std::map<NodeType, std::shared_ptr<Node>> _upper_nodes;
		std::map<NodeType, std::shared_ptr<Node>> _lower_nodes;

	private:
		NodeType _node_type = NodeType::Unknown;
		NodeState _state = NodeState::Ready;
	};
}  // namespace pub