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
	void SetChainCertificate(const std::shared_ptr<Certificate> &certificate);

	void SetTlsWriteToResponse(bool tls_write_to_response)
    {
        _tls_write_to_response = tls_write_to_response;
    }
protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;

protected:
	std::shared_ptr<Certificate> _local_certificate = nullptr;
	std::shared_ptr<Certificate> _chain_certificate = nullptr;
	bool _tls_write_to_response = false;
};
