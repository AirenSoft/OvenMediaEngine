//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "https_server.h"

#include "http_private.h"

// Reference: https://wiki.mozilla.org/Security/Server_Side_TLS

// Modern compatibility
#define HTTP_MODERN_COMPATIBILITY "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256"
// Intermediate compatibility
#define HTTP_INTERMEDIATE_COMPATIBILITY "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA:ECDHE-ECDSA-DES-CBC3-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:DES-CBC3-SHA:!DSS"
// Backward compatibility
#define HTTP_BACKWARD_COMPATIBILITY "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:ECDHE-RSA-DES-CBC3-SHA:ECDHE-ECDSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:DES-CBC3-SHA:HIGH:SEED:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!RSAPSK:!aDH:!aECDH:!EDH-DSS-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA:!SRP"

bool HttpsServer::SetCertificate(const std::shared_ptr<info::Certificate> &certificate)
{
	if ((_certificate != nullptr) && (_certificate != certificate))
	{
		// TODO(dimiden): Cannot change certificate - Currently, it's a limitation of HttpServer
		return false;
	}

	_certificate = certificate;

	return true;
}

void HttpsServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	auto client = ProcessConnect(remote);

	if (client != nullptr)
	{
		if (_certificate == nullptr)
		{
			return;
		}

		auto tls_data = std::make_shared<ov::TlsData>(
			ov::TlsData::Method::TlsServerMethod,
			_certificate->GetCertificate(), _certificate->GetChainCertificate(),
			HTTP_INTERMEDIATE_COMPATIBILITY);

		tls_data->SetWriteCallback([remote](const void *data, size_t length) -> ssize_t {
			return remote->Send(data, length);
		});

		client->GetRequest()->SetTlsData(tls_data);
		client->GetResponse()->SetTlsData(tls_data);
	}
}

void HttpsServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	auto client = FindClient(remote);

	if (client == nullptr)
	{
		// This can be called in situations where the client closes the connection from the server at the same time as the data is sent
		return;
	}

	auto request = client->GetRequest();
	auto tls_data = request->GetTlsData();

	if (tls_data != nullptr)
	{
		std::shared_ptr<const ov::Data> plain_data;

		if (tls_data->Decrypt(data, &plain_data))
		{
			if ((plain_data != nullptr) && (plain_data->GetLength() > 0))
			{
				// plain_data is HTTP data

				// Use the decrypted data
				HttpServer::ProcessData(client, plain_data);
			}
			else
			{
				// Need more data to decrypt the data
			}

			return;
		}

		// Error
		logtd("Could not decrypt data");
	}
	else
	{
		OV_ASSERT(false, "TlsData must not be null");
	}

	client->GetResponse()->Close();
}
