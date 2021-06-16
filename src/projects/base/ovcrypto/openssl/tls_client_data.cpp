//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "tls_client_data.h"

#define OV_LOG_TAG "TLS"

namespace ov
{
	TlsClientData::TlsClientData(Method method)
	{
		TlsCallback callback =
			{
				.create_callback = [](Tls *tls, SSL_CTX *context) -> bool {
					return true;
				},

				.read_callback = std::bind(&TlsClientData::OnTlsRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
				.write_callback = std::bind(&TlsClientData::OnTlsWrite, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
				.destroy_callback = nullptr,
				.ctrl_callback = std::bind(&TlsClientData::OnTlsCtrl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
				.verify_callback = nullptr};

		const SSL_METHOD *tls_method = nullptr;
		std::shared_ptr<Error> error;

		switch (method)
		{
			case Method::Tls:
				tls_method = TLS_client_method();
				break;
		}

		if (error == nullptr)
		{
			if (_tls.InitializeClientTls(tls_method, callback))
			{
				_method = method;
				_state = State::WaitingForConnect;
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

	TlsClientData::~TlsClientData()
	{
		_tls.Uninitialize();
	}

	void TlsClientData::SetIoCallback(const std::shared_ptr<TlsClientDataIoCallback> &callback)
	{
		_io_callback = callback;
	}

	std::shared_ptr<const OpensslError> TlsClientData::Connect()
	{
		_state = State::Connecting;

		auto error = _tls.Connect();

		if (error == nullptr)
		{
			logtd("Subject: %s, Issuer: %s", _tls.GetSubjectName().CStr(), _tls.GetIssuerName().CStr());
			_state = State::Connected;
		}

		return error;
	}

	std::shared_ptr<const Data> TlsClientData::Decrypt()
	{
		if (_state == State::Invalid)
		{
			logtd("Invalid state");
			return nullptr;
		}

		return _tls.Read();
	}

	bool TlsClientData::Encrypt(const std::shared_ptr<const Data> &plain_data)
	{
		if (_state != State::Connected)
		{
			// Before encrypting data, key exchange must be done first
			logtd("Invalid state: %d", _state);
			return false;
		}

		logtd("Trying to encrypt the data for TLS\n%s", plain_data->Dump(32).CStr());

		size_t written_bytes = 0;

		// Tls::Write() -> SSL_write() -> Tls::TlsWrite() -> BIO_get_data()::write_callback -> TlsClientData::OnTlsWrite()
		if (_tls.Write(plain_data, &written_bytes) == SSL_ERROR_NONE)
		{
			return true;
		}

		return false;
	}

	ssize_t TlsClientData::OnTlsRead(Tls *tls, void *buffer, size_t length)
	{
		logtd("Trying to read %zu bytes from I/O callback...", length);

		if (_io_callback != nullptr)
		{
			return _io_callback->OnTlsReadData(buffer, length);
		}

		return -1;
	}

	ssize_t TlsClientData::OnTlsWrite(Tls *tls, const void *data, size_t length)
	{
		logtd("Trying to send encrypted data to I/O callback\n%s", ov::Dump(data, length, 32).CStr());

		if (_io_callback != nullptr)
		{
			return _io_callback->OnTlsWriteData(data, length);
		}

		return -1;
	}

	long TlsClientData::OnTlsCtrl(Tls *tls, int cmd, long num, void *arg)
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