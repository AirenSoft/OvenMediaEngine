//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_server.h"

class HttpsServer : public HttpServer
{
public:
	// Set Local Certificate
	void SetLocalCertificate(const std::shared_ptr<Certificate> &certificate);

protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(ov::Socket *remote) override;
	void OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;

protected:
	std::shared_ptr<Certificate> _local_certificate = nullptr;
};
