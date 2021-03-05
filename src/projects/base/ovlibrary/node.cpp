//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "node.h"

namespace ov
{
	Node::Node(NodeType node_type)
	{
		_node_type = node_type;
	}

	Node::~Node()
	{
		
	}

	NodeType Node::GetNodeType()
	{
		return _node_type;
	}

	bool Node::Start()
	{
		_state = NodeState::Started;
		return true;
	}

	bool Node::Stop()
	{
		_state = NodeState::Stopped;

		_upper_nodes.clear();
		_lower_nodes.clear();

		return true;
	}

	Node::NodeState Node::GetState()
	{
		return _state;
	}

	std::shared_ptr<Node> Node::GetUpperNode()
	{
		if (_upper_nodes.empty())
		{
			return nullptr;
		}

		return _upper_nodes.begin()->second;
	}

	std::shared_ptr<Node> Node::GetLowerNode()
	{
		if (_lower_nodes.empty())
		{
			return nullptr;
		}

		return _lower_nodes.begin()->second;
	}

	std::shared_ptr<Node> Node::GetUpperNode(NodeType node_type)
	{
		if (_upper_nodes.count(node_type) == 0)
		{
			return nullptr;
		}

		return _upper_nodes[node_type];
	}

	std::shared_ptr<Node> Node::GetLowerNode(NodeType node_type)
	{
		if (_lower_nodes.count(node_type) == 0)
		{
			return nullptr;
		}

		return _lower_nodes[node_type];
	}

	void Node::RegisterUpperNode(const std::shared_ptr<Node> &node)
	{
		if (!node)
		{
			return;
		}

		_upper_nodes[node->GetNodeType()] = node;
	}

	void Node::RegisterLowerNode(const std::shared_ptr<Node> &node)
	{
		if (!node)
		{
			return;
		}

		_lower_nodes[node->GetNodeType()] = node;
	}
}  // namespace pub