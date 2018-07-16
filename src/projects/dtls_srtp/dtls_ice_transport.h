//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//  DTLS와 연결될 때 사용하는 IcePort Session Node
//==============================================================================

#pragma once

#include <base/publisher/session_node.h>
#include "ice/ice_port.h"


class DtlsIceTransport : public SessionNode
{
public:
	DtlsIceTransport(uint32_t node_id, std::shared_ptr<Session> session, std::shared_ptr<IcePort> ice_port);
	virtual ~DtlsIceTransport();

	// Implement SessionNode Interface
	// 데이터를 upper에서 받는다. lower node로 보낸다.
	bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	// 데이터를 lower에서 받는다. upper node로 보낸다.
	bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;

private:
	std::shared_ptr<IcePort> _ice_port;
};
