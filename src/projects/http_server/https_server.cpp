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

void HttpsServer::SetLocalCertificate(const std::shared_ptr<Certificate> &certificate)
{
	_local_certificate = certificate;
}

void HttpsServer::SetChainCertificate(const std::shared_ptr<Certificate> &certificate)
{
	_chain_certificate = certificate;
}

void HttpsServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	HttpServer::OnConnected(remote);

	auto client = FindClient(remote);

	OV_ASSERT2(client != nullptr);

	// Prepare TLS for client
	ov::TlsCallback callback =
		{
			.create_callback = [](ov::Tls *tls, SSL_CTX *context) -> bool
			{
				return true;
			},

			.read_callback = std::bind(&HttpClient::TlsRead, client, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			.write_callback = std::bind(&HttpClient::TlsWrite, client, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			.destroy_callback = nullptr,
			.ctrl_callback = [](ov::Tls *tls, int cmd, long num, void *arg) -> long
			{
				logtd("[TLS] Ctrl: %d, %ld, %p", cmd, num, arg);

				switch(cmd)
				{
					case BIO_CTRL_RESET:
					case BIO_CTRL_WPENDING:
					case BIO_CTRL_PENDING:
						return 0;

					case BIO_CTRL_FLUSH:
						return 1;

					default:
						return 0;
				}
			},
			.verify_callback = nullptr
		};

	auto tls = std::make_shared<ov::Tls>();

	if(tls->Initialize(TLS_server_method(), _local_certificate, _chain_certificate, HTTP_INTERMEDIATE_COMPATIBILITY, callback) == false)
	{
        logte("Tls initialize fail");

		// TODO: Disconnect
		return;
	}

	client->SetTls(tls);
    client->SetTlsWriteToResponse(_tls_write_to_response);
}

void HttpsServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	auto client = FindClient(remote);

	if(client == nullptr)
	{
		// client possible nullptr in thread 
		//OV_ASSERT2(false);
		logtw("http client is nullptr");
		return;
	}

	// Need to decrypt using TLS

	// * Data flow:
	//   1. HttpClient::SetTlsData() -> // save data to HttpClient::_tls_read_data
	//   2. ov::Tls::Read() ->
	//   3. SSL_read() ->
	//   4. HttpClient::TlsRead() -> // read data from HttpClient::_tls_read_data
	//   5. (ov::Tls::Read returns decrypted data)
	logtd("Trying to set data for TLS\n%s", data->Dump(32).CStr());
	client->SetTlsData(data);
	auto tls = client->GetTls();

	if(tls == nullptr)
    {
        logte("Tls is null: %s", remote->ToString().CStr());
        HttpServer::Disconnect(client);
        return;
    }

	if(client->IsAccepted() == false)
	{
		logtd("Trying to accept TLS...");
		int result = tls->Accept();

		logtd("Accept result: %d", result);

		switch(result)
		{
			case SSL_ERROR_NONE:
				client->MarkAsAccepted();

				logti("Accepted");
				break;

			case SSL_ERROR_WANT_READ:
				// Need more data to decrypt
				break;


			default:
				logte("An error occurred while accept client: %s", remote->ToString().CStr());
				HttpServer::Disconnect(client);
				return;
		}
	}

	if(client->IsAccepted())
	{
		logtd("Trying to read data from TLS module...");

		auto plain_data = tls->Read();

		if((plain_data != nullptr) && (plain_data->GetLength() > 0))
		{
			// Use the decrypted data
			HttpServer::ProcessData(client, plain_data);
		}
		else
		{
			// Cannot decrypt data
		}
	}
}
