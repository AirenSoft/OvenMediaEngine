//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "https_client.h"
#include "http_private.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server.h"

HttpsClient::HttpsClient(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &remote)
	: HttpClient(server, remote)
{
	OV_ASSERT2(_server != nullptr);
	OV_ASSERT2(_remote != nullptr);
}

void HttpsClient::SetTls(const std::shared_ptr<ov::Tls> &tls)
{
	logtd("Now HttpsClient(%p) uses TLS(%p) (previous: %p)", this, tls.get(), _tls.get());

	OV_ASSERT2((tls == nullptr) || (_tls == nullptr));

	_tls = tls;
}

std::shared_ptr<ov::Data> HttpsClient::DecryptData(const std::shared_ptr<const ov::Data> &data)
{
	std::lock_guard<__decltype(_tls_data_mutex)> guard(_tls_data_mutex);

	if (_tls == nullptr)
	{
		// The connection is closed while receiving the data from the client
		Close();
		return nullptr;
	}

	if (data == nullptr)
	{
		logte("Data must be not null");
		OV_ASSERT2(data != nullptr);
		return nullptr;
	}

	// Decryption order:
	//   1. HttpsClient::SetTlsData() -> // Save the received data to HttpsClient::_tls_read_data
	//   2. ov::Tls::Read() ->
	//   3. SSL_read() ->
	//   4. HttpsClient::TlsRead() -> // Read data from HttpClient::_tls_read_data
	//   5. (ov::Tls::Read returns the decrypted data)

	logtd("Trying to decrypt the data for TLS\n%s", data->Dump(32).CStr());

	if ((_tls_data == nullptr) || _tls_data->IsEmpty())
	{
		_tls_data = data;
	}
	else
	{
		// Append the data
		auto temp_data = _tls_data->Clone();
		temp_data->Append(data);
		_tls_data = temp_data;
	}

	if (_is_tls_accepted == false)
	{
		logtd("Trying to accept TLS...");

		std::lock_guard<__decltype(_tls_mutex)> lock(_tls_mutex);

		int result = _tls->Accept();

		switch (result)
		{
			case SSL_ERROR_NONE:
				logti("Accepted");
				_is_tls_accepted = true;
				break;

			case SSL_ERROR_WANT_READ:
				logtd("Need more data to accept the request");
				break;

			default:
				logte("An error occurred while accept client: %s", _remote->ToString().CStr());
				Close();
				return nullptr;
		}
	}

	if (_is_tls_accepted)
	{
		std::lock_guard<__decltype(_tls_mutex)> lock(_tls_mutex);

		logtd("Trying to read data from TLS module...");
		auto plain_data = _tls->Read();

		logtd("Decrypted data\n%s", plain_data->Dump().CStr());

		return std::move(plain_data);
	}

	// Need more data to accept the request
	return nullptr;
}

ssize_t HttpsClient::TlsRead(ov::Tls *tls, void *buffer, size_t length)
{
	OV_ASSERT2(tls != nullptr);
	OV_ASSERT2(buffer != nullptr);

	if (_tls_data == nullptr)
	{
		OV_ASSERT2(false);
		return -1LL;
	}

	std::lock_guard<__decltype(_tls_mutex)> lock(_tls_mutex);

	auto bytes_to_copy = std::min(length, _tls_data->GetLength());

	logtd("Trying to read %zu bytes from TLS data buffer...", bytes_to_copy);

	::memcpy(buffer, _tls_data->GetData(), bytes_to_copy);

	_tls_data = _tls_data->Subdata(bytes_to_copy);

	logtd("%zu bytes is remained in TLS data buffer", _tls_data->GetLength());

	return bytes_to_copy;
}

ssize_t HttpsClient::TlsWrite(ov::Tls *tls, const void *data, size_t length)
{
	if (data == nullptr)
	{
		OV_ASSERT2(data != nullptr);
		return -1LL;
	}

	std::lock_guard<__decltype(_tls_mutex)> lock(_tls_mutex);

	// Need to send the data to the client which is sent by ov::Tls
	auto sent_bytes = _remote->Send(data, length);

	return sent_bytes;
}

bool HttpsClient::Send(const std::shared_ptr<const ov::Data> &data)
{
	std::lock_guard<__decltype(_tls_mutex)> lock(_tls_mutex);

	size_t written_bytes = 0;

	auto payload = data->GetData();
	auto len = data->GetLength();

	if (_tls->Write(payload, len, &written_bytes) == SSL_ERROR_NONE)
	{
		return written_bytes == data->GetLength();
	}

	return false;
}

bool HttpsClient::Close()
{
	SetTls(nullptr);

	return HttpClient::Close();
}
