//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/session_node.h>
#include <base/common_types.h>
#include "rtp_rtcp/rtp_rtcp.h"
#include "srtp_adapter.h"

class SrtpTransport : public SessionNode
{
public:
	SrtpTransport(uint32_t node_id, std::shared_ptr<Session> session);
	virtual ~SrtpTransport();

	// 데이터를 upper에서 받는다. lower node로 보낸다.
	bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;

    // srtcp transfer
    bool SendRtcpData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data);

	// 데이터를 lower에서 받는다. upper node로 보낸다.
	bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;


	bool SetKeyMeterial(uint64_t crypto_suite,
						std::shared_ptr<ov::Data> server_key, std::shared_ptr<ov::Data> client_key);

private:
	std::shared_ptr<SrtpAdapter>		_send_session;
	std::shared_ptr<SrtpAdapter>		_recv_session;
};
