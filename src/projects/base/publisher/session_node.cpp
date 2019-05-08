//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "publisher_private.h"
#include "session_node.h"

SessionNode::SessionNode(uint32_t id, SessionNodeType node_type, std::shared_ptr<Session> session)
{
	_node_id = id;
	_node_type = node_type;
	_session = session;
	_state = NodeState::Ready;
}

SessionNode::~SessionNode()
{
	_upper_nodes.clear();
	_lower_nodes.clear();
}

std::shared_ptr<Session> SessionNode::GetSession()
{
	return _session;
}

uint32_t SessionNode::GetId()
{
	return _node_id;
}

SessionNodeType SessionNode::GetNodeType()
{
	return _node_type;
}


bool SessionNode::Start()
{
	_state = NodeState::Started;
	return true;
}

bool SessionNode::Stop()
{
	_state = NodeState::Stopped;
	// Because it is a cross-reference to the parent, it forces the removal to free memory.
	_session.reset();
	return true;
}

SessionNode::NodeState SessionNode::GetState()
{
	return _state;
}

// 하나만 연결되어 있을 때 사용한다.
std::shared_ptr<SessionNode> SessionNode::GetUpperNode()
{
	if(_upper_nodes.empty())
	{
		return nullptr;
	}
	return _upper_nodes.begin()->second;
}

std::shared_ptr<SessionNode> SessionNode::GetLowerNode()
{
	if(_lower_nodes.empty())
	{
		return nullptr;
	}
	return _lower_nodes.begin()->second;
}

std::shared_ptr<SessionNode> SessionNode::GetUpperNode(SessionNodeType node_type)
{
	if(_upper_nodes.count(node_type) == 0)
	{
		return nullptr;
	}

	return _upper_nodes[node_type];
}

std::shared_ptr<SessionNode> SessionNode::GetLowerNode(SessionNodeType node_type)
{
	if(_lower_nodes.count(node_type) == 0)
	{
		return nullptr;
	}

	return _lower_nodes[node_type];
}

void SessionNode::RegisterUpperNode(std::shared_ptr<SessionNode> node)
{
	if(!node)
	{
		return;
	}

	_upper_nodes[node->GetNodeType()] = node;
}

void SessionNode::RegisterLowerNode(std::shared_ptr<SessionNode> node)
{
	if(!node)
	{
		return;
	}

	_lower_nodes[node->GetNodeType()] = node;
}
