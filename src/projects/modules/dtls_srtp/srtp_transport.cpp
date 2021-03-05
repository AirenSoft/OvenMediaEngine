//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srtp_transport.h"
#include "dtls_transport.h"

#define OV_LOG_TAG "SRTP"

SrtpTransport::SrtpTransport()
	: ov::Node(NodeType::Srtp)
{

}

SrtpTransport::~SrtpTransport()
{
}

bool SrtpTransport::Stop()
{
	if(_send_session != nullptr)
	{
		_send_session->Release();
	}

	if(_recv_session != nullptr)
	{
		_recv_session->Release();
	}

	return Node::Stop();
}

bool SrtpTransport::SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	if(!_send_session)
	{
		return false;
	}
	
	if(from_node == NodeType::Rtp)
	{
		if(!_send_session->ProtectRtp(data))
		{
			return false;
		}
	}
	else if(from_node == NodeType::Rtcp)
	{
		 if(!_send_session->ProtectRtcp(data))
		 {
			return false;
		 }
	}
	else
	{
		return false;
	}

	// To DTLS transport
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}
	
	logtd("SrtpTransport Send next node : %d", data->GetLength());
	return node->SendData(GetNodeType(), data);
}

bool SrtpTransport::OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	if(_recv_session == nullptr)
	{
		return false;
	}

	auto decode_data = data->Clone();
    if(!_recv_session->UnprotectRtcp(decode_data))
    {
        logtd("stcp unprotected fail");
        return false;
    }

	// To RTP_RTCP
    auto node = GetUpperNode();
    if(node == nullptr)
    {
        return false;
    }
    node->OnDataReceived(GetNodeType(), decode_data);

	return true;
}

// Initialize SRTP
bool SrtpTransport::SetKeyMeterial(uint64_t crypto_suite, std::shared_ptr<ov::Data> server_key, std::shared_ptr<ov::Data> client_key)
{
	if(_send_session || _recv_session)
	{
		return false;
	}

	logtd("Try to set key meterial");

	_send_session = std::make_shared<SrtpAdapter>();
	if(_send_session == nullptr)
	{
		logte("Create srtp adapter failed");
		return false;
	}

	if(!_send_session->SetKey(ssrc_any_outbound, crypto_suite, server_key))
	{
		return false;
	}

	_recv_session = std::make_shared<SrtpAdapter>();
	if(_recv_session == nullptr)
	{
		_send_session.reset();
		_send_session = nullptr;
		return false;
	}

	if(!_recv_session->SetKey(ssrc_any_inbound, crypto_suite, client_key))
	{
		return false;
	}

	return true;
}