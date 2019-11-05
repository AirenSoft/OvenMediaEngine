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
#include "https_client.h"

class HttpsServer : public HttpServer
{
public:
	// Set Local Certificate
	void SetLocalCertificate(const std::shared_ptr<Certificate> &certificate);
	void SetChainCertificate(const std::shared_ptr<Certificate> &certificate);

protected:
	// Note: This does NOT override HttpServer::FindClient()
	std::shared_ptr<HttpsClient> FindClient(const std::shared_ptr<ov::Socket> &remote);

	//--------------------------------------------------------------------
	// Overriding functions of HttpServer
	//--------------------------------------------------------------------
	std::shared_ptr<HttpClient> CreateClient(const std::shared_ptr<ov::ClientSocket> &remote) override;

	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;

protected:
	std::shared_ptr<Certificate> _local_certificate = nullptr;
	std::shared_ptr<Certificate> _chain_certificate = nullptr;
};
