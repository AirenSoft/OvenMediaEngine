//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/info/host.h"
#include "http_server.h"
#include "orchestrator/orchestrator.h"

class HttpsServer : public HttpServer
{
public:
	HttpsServer(const char *server_name)
		: HttpServer(server_name)
	{
	}

	// TODO(Dimiden): OME doesn't support SNI yet, so OME can handle only one certificate.
	bool SetCertificate(const std::shared_ptr<info::Certificate> &certificate);

protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;

protected:
	std::shared_ptr<info::Certificate> _certificate;
};
