//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dtls_ice_transport.h"

#define OV_LOG_TAG "DTLS.ICE"

DtlsIceTransport::DtlsIceTransport(session_id_t session_id, const std::shared_ptr<IcePort> &ice_port)
	: Node(NodeType::Ice)
{
	_session_id = session_id;
	_ice_port = ice_port;
}

DtlsIceTransport::~DtlsIceTransport()
{
}

// Implement Node Interface
bool DtlsIceTransport::SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	logtd("DtlsIceTransport Send by ice port : %d", data->GetLength());
	_ice_port->Send(_session_id, data);

	return true;
}

bool DtlsIceTransport::OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	auto node = GetUpperNode();
	if(node == nullptr)
	{
		return false;
	}

	node->OnDataReceived(from_node, data);
	
	return true;
}