//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./tls.h"
#include "openssl_error.h"

namespace ov
{
	class TlsServerData
	{
	public:
		using WriteCallback = std::function<ssize_t(const void *data, int64_t length)>;

		enum class Method
		{
			// TLS_server_method()
			Tls,
			// DTLS_server_method()
			Dtls,
		};

		enum class State
		{
			Invalid,

			WaitingForAccept,
			Accepted,
		};

		TlsServerData(Method method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const String &cipher_list);

		~TlsServerData();

		State GetState() const
		{
			return _state;
		}

		const WriteCallback &GetWriteCallback() const
		{
			return _write_callback;
		}

		// This callback is called when TLS negotiation is in progress
		void SetWriteCallback(WriteCallback write_callback)
		{
			_write_callback = write_callback;
		}

		// plain_data can be null even if successful (It indicates accepting a new client)
		bool Decrypt(const std::shared_ptr<const Data> &cipher_data, std::shared_ptr<const Data> *plain_data);
		// cipher_data can be null even if successful (It indicates accepting a new client)
		bool Encrypt(const std::shared_ptr<const Data> &plain_data, std::shared_ptr<const Data> *cipher_data);

		size_t GetDataLength() const;
		std::shared_ptr<const Data> GetData() const;

	protected:
		//--------------------------------------------------------------------
		// Called by TLS module
		//--------------------------------------------------------------------
		// Tls::Read() -> SSL_read() -> Tls::TlsRead() -> BIO_get_data()::read_callback -> TlsServerData::OnTlsRead()
		ssize_t OnTlsRead(Tls *tls, void *buffer, size_t length);
		// Tls::Write() -> SSL_write() -> Tls::TlsWrite() -> BIO_get_data()::write_callback -> TlsServerData::OnTlsWrite()
		ssize_t OnTlsWrite(Tls *tls, const void *data, size_t length);

		long OnTlsCtrl(ov::Tls *tls, int cmd, long num, void *arg);

		State _state = State::Invalid;
		Method _method;

		Tls _tls;
		std::mutex _data_mutex;
		WriteCallback _write_callback;

		std::shared_ptr<Data> _cipher_data;
		std::shared_ptr<Data> _plain_data;
	};
}  // namespace ov