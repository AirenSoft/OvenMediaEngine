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

DtlsIceTransport::DtlsIceTransport(uint32_t node_id, std::shared_ptr<pub::Session> session, std::shared_ptr<IcePort> ice_port)
	: SessionNode(node_id, pub::SessionNodeType::Ice, session)
{
	_ice_port = ice_port;
}

DtlsIceTransport::~DtlsIceTransport()
{
}

// Implement SessionNode Interface
bool DtlsIceTransport::SendData(pub::SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	logtd("DtlsIceTransport Send by ice port : %d", data->GetLength());
	_ice_port->Send(GetSession(), data);

	return true;
}

bool DtlsIceTransport::OnDataReceived(pub::SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
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