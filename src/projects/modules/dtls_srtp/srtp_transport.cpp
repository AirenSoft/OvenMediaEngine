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

SrtpTransport::SrtpTransport(uint32_t node_id, std::shared_ptr<pub::Session> session)
	: SessionNode(node_id, pub::SessionNodeType::Srtp, session)
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

	return SessionNode::Stop();
}

// 데이터를 upper(RTP_RTCP)에서 받는다. lower node(DTLS)로 보낸다.
bool SrtpTransport::SendData(pub::SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	// Node 시작 전에는 아무것도 하지 않는다.
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	if(!_send_session)
	{
		return false;
	}
	
	if(from_node == pub::SessionNodeType::Rtp)
	{
		if(!_send_session->ProtectRtp(data))
		{
			return false;
		}
	}
	else if(from_node == pub::SessionNodeType::Rtcp)
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

	// DTLS로 보낸다.
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}
	//logtd("SrtpTransport Send next node : %d", data->GetLength());
	return node->SendData(GetNodeType(), data);
}

// 데이터를 lower(DTLS)에서 받는다. upper node(RTP_RTCP)로 보낸다.
bool SrtpTransport::OnDataReceived(pub::SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// Node 시작 전에는 아무것도 하지 않는다.
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	// TODO(getroot): _recv_session가 생성되기 전에 호출되는 경우가 있음. 확인 필요
	// auto decode_data = data->Clone();

    // if(!_recv_session->UnprotectRtcp(decode_data))
    // {
    //     logtd("stcp unprotected fail");
    //     return false;
    // }

	// // pass to rtcp
    // auto node = GetUpperNode();

    // if(node == nullptr)
    // {
    //     return false;
    // }
    // node->OnDataReceived(GetNodeType(), decode_data);

	return true;
}

// SRTP 를 초기화 한다.
bool SrtpTransport::SetKeyMeterial(uint64_t crypto_suite, std::shared_ptr<ov::Data> server_key, std::shared_ptr<ov::Data> client_key)
{
	// 이미 Session을 만들었다면 다시 하지 않는다.
	// TODO: 향후 재협상을 개발해야 한다.
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