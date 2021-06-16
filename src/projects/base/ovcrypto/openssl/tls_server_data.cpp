//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "tls_server_data.h"

#define OV_LOG_TAG "TLS"

namespace ov
{
	TlsServerData::TlsServerData(Method method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const String &cipher_list)
	{
		TlsCallback callback =
			{
				.create_callback = [](Tls *tls, SSL_CTX *context) -> bool {
					return true;
				},

				.read_callback = std::bind(&TlsServerData::OnTlsRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
				.write_callback = std::bind(&TlsServerData::OnTlsWrite, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
				.destroy_callback = nullptr,
				.ctrl_callback = std::bind(&TlsServerData::OnTlsCtrl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
				.verify_callback = nullptr};

		const SSL_METHOD *tls_method = nullptr;
		std::shared_ptr<Error> error;

		switch (method)
		{
			case Method::Tls:
				tls_method = TLS_server_method();
				break;

			case Method::Dtls:
				tls_method = DTLS_server_method();
				break;
		}

		if (error == nullptr)
		{
			if (_tls.InitializeServerTls(tls_method, certificate, chain_certificate, cipher_list, callback))
			{
				_method = method;
				_state = State::WaitingForAccept;
			}
			else
			{
				error = OpensslError::CreateErrorFromOpenssl();
			}
		}

		if (error != nullptr)
		{
			logte("Could not initialize TLS: %s", error->ToString().CStr());
			_state = State::Invalid;
		}
	}

	TlsServerData::~TlsServerData()
	{
		_tls.Uninitialize();
	}

	bool TlsServerData::Decrypt(const std::shared_ptr<const Data> &cipher_data, std::shared_ptr<const Data> *plain_data)
	{
		if (_state == State::Invalid)
		{
			logtd("Invalid state");
			return false;
		}

		logtd("Trying to decrypt the data for TLS\n%s", cipher_data->Dump(32).CStr());

		if ((_cipher_data == nullptr) || _cipher_data->IsEmpty())
		{
			_cipher_data = cipher_data->Clone();
		}
		else
		{
			// Append the data
			if (_cipher_data->Append(cipher_data) == false)
			{
				logtd("Could not append data");
				return false;
			}
		}

		switch (_state)
		{
			case State::Invalid:
				OV_ASSERT2(false);
				return false;

			case State::WaitingForAccept: {
				logtd("Trying to accept TLS...");

				int result = _tls.Accept();

				switch (result)
				{
					case SSL_ERROR_NONE:
						logtd("Accepted");
						_state = State::Accepted;
						break;

					case SSL_ERROR_WANT_READ:
						logtd("Need more data to accept the request");
						break;

					default:
						logte("An error occurred while accept TLS connection");
						return false;
				}

				break;
			}

			case State::Accepted: {
				logtd("Trying to read data from TLS module...");

				// Tls::Read() -> SSL_read() -> Tls::TlsRead() -> BIO_get_data()::read_callback -> TlsServerData::OnTlsRead()
				auto decrypted = _tls.Read();

				logtd("Decrypted data\n%s", decrypted->Dump().CStr());

				*plain_data = decrypted;

				break;
			}
		}

		return true;
	}

	bool TlsServerData::Encrypt(const std::shared_ptr<const Data> &plain_data, std::shared_ptr<const Data> *cipher_data)
	{
		if (_state != State::Accepted)
		{
			// Before encrypting data, key exchange must be done first
			logtd("Invalid state: %d", _state);
			return false;
		}

		logtd("Trying to encrypt the data for TLS\n%s", plain_data->Dump(32).CStr());

		size_t written_bytes = 0;

		// Tls::Write() -> SSL_write() -> Tls::TlsWrite() -> BIO_get_data()::write_callback -> TlsServerData::OnTlsWrite()
		if (_tls.Write(plain_data, &written_bytes) == SSL_ERROR_NONE)
		{
			*cipher_data = std::move(_plain_data);
			return true;
		}

		return false;
	}

	ssize_t TlsServerData::OnTlsRead(Tls *tls, void *buffer, size_t length)
	{
		if (_cipher_data == nullptr)
		{
			OV_ASSERT2(false);
			return -1;
		}

		auto bytes_to_copy = std::min(length, _cipher_data->GetLength());

		logtd("Trying to read %zu bytes from TLS data buffer...", bytes_to_copy);

		::memcpy(buffer, _cipher_data->GetData(), bytes_to_copy);
		_cipher_data = _cipher_data->Subdata(bytes_to_copy);

		logtd("%zu bytes is remained in TLS data buffer", _cipher_data->GetLength());

		return bytes_to_copy;
	}

	ssize_t TlsServerData::OnTlsWrite(Tls *tls, const void *data, size_t length)
	{
		if (_state == State::WaitingForAccept)
		{
			if (_write_callback != nullptr)
			{
				return _write_callback(data, length);
			}

			OV_ASSERT2(false);
			return -1LL;
		}
		else
		{
			if (_plain_data == nullptr)
			{
				_plain_data = std::make_shared<Data>(data, length);
			}
			else
			{
				_plain_data->Append(data, length);
			}
		}

		return length;
	}

	long TlsServerData::OnTlsCtrl(Tls *tls, int cmd, long num, void *arg)
	{
		logtd("[TLS] Ctrl: %d, %ld, %p", cmd, num, arg);

		switch (cmd)
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
	}
}  // namespace ov