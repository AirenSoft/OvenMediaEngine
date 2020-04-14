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
#include "modules/rtp_rtcp/rtp_rtcp.h"
#include "srtp_adapter.h"

class SrtpTransport : public pub::SessionNode
{
public:
	SrtpTransport(uint32_t node_id, std::shared_ptr<pub::Session> session);
	virtual ~SrtpTransport();

	bool Stop() override;

	// 데이터를 upper에서 받는다. lower node로 보낸다.
	bool SendData(pub::SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;

	// 데이터를 lower에서 받는다. upper node로 보낸다.
	bool OnDataReceived(pub::SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;


	bool SetKeyMeterial(uint64_t crypto_suite,
						std::shared_ptr<ov::Data> server_key, std::shared_ptr<ov::Data> client_key);

private:
	std::shared_ptr<SrtpAdapter>		_send_session;
	std::shared_ptr<SrtpAdapter>		_recv_session;
};
