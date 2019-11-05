//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_client.h"

class HttpsServer;

class HttpsClient : public HttpClient
{
public:
	friend class HttpServer;
	friend class HttpsServer;

	HttpsClient(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &remote);

	bool Send(const std::shared_ptr<const ov::Data> &data) override;
	bool Close() override;

protected:
	//--------------------------------------------------------------------
	// APIs which is related to TLS
	//--------------------------------------------------------------------
	void SetTls(const std::shared_ptr<ov::Tls> &tls);

	std::shared_ptr<ov::Data> DecryptData(const std::shared_ptr<const ov::Data> &data);

	//--------------------------------------------------------------------
	// Called by TLS module
	//--------------------------------------------------------------------
	// Tls::Read() -> SSL_read -> Tls::TlsRead() -> BIO_get_data()::read_callback -> HttpsClient::TlsRead()
	ssize_t TlsRead(ov::Tls *tls, void *buffer, size_t length);
	// Tls::Write() ->  SSL_write -> Tls::TlsWrite() -> BIO_get_data()::write_callback -> HttpsClient::TlsWrite()
	ssize_t TlsWrite(ov::Tls *tls, const void *data, size_t length);

	std::recursive_mutex _tls_mutex;
	std::shared_ptr<ov::Tls> _tls = nullptr;
	bool _is_tls_accepted = false;

	std::recursive_mutex _tls_data_mutex;
	std::shared_ptr<const ov::Data> _tls_data = nullptr;
};
