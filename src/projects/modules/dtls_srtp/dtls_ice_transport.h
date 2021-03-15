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

#include <base/ovlibrary/node.h>
#include <base/info/session.h>
#include "modules/ice/ice_port.h"

class DtlsIceTransport : public ov::Node
{
public:
	DtlsIceTransport(session_id_t session_id, const std::shared_ptr<IcePort> &ice_port);
	virtual ~DtlsIceTransport();

	// Implement ov::Node Interface
	bool SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	bool OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data) override;

private:
	session_id_t _session_id;
	std::shared_ptr<IcePort> _ice_port;
};
