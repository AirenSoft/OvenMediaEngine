//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./tls.h"
#include "openssl_error.h"

namespace ov
{
	class TlsClientDataIoCallback
	{
	public:
		virtual ssize_t OnTlsReadData(void *data, int64_t length) = 0;
		virtual ssize_t OnTlsWriteData(const void *data, int64_t length) = 0;
	};

	class TlsClientData
	{
	public:
		enum class Method
		{
			// TLS_client_method()
			Tls
		};

		enum class State
		{
			Invalid,

			WaitingForConnect,
			Connecting,
			Connected,
		};

		TlsClientData(Method method);

		~TlsClientData();

		State GetState() const
		{
			return _state;
		}

		void SetIoCallback(const std::shared_ptr<TlsClientDataIoCallback> &callback);

		std::shared_ptr<const OpensslError> Connect();

		/// Read via _io_callback->OnTlsReadData() and decrypt it, and returns it
		///
		/// @returns Returns decrypted data if there is no error, otherwise returns nullptr
		std::shared_ptr<const Data> Decrypt();
		// Encrypt plain_data, and send via _io_callback->OnTlsWriteData()
		bool Encrypt(const std::shared_ptr<const Data> &plain_data);

		size_t GetDataLength() const;
		std::shared_ptr<const Data> GetData() const;

	protected:
		//--------------------------------------------------------------------
		// Called by TLS module
		//--------------------------------------------------------------------
		// Tls::Read() -> SSL_read() -> Tls::TlsRead() -> BIO_get_data()::read_callback -> TlsClientData::OnTlsRead()
		ssize_t OnTlsRead(Tls *tls, void *buffer, size_t length);
		// Tls::Write() -> SSL_write() -> Tls::TlsWrite() -> BIO_get_data()::write_callback -> TlsClientData::OnTlsWrite()
		ssize_t OnTlsWrite(Tls *tls, const void *data, size_t length);

		long OnTlsCtrl(ov::Tls *tls, int cmd, long num, void *arg);

		State _state = State::Invalid;
		Method _method;

		std::shared_ptr<TlsClientDataIoCallback> _io_callback;

		Tls _tls;
		std::mutex _data_mutex;
	};
}  // namespace ov