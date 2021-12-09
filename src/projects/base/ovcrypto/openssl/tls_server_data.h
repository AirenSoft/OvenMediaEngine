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

		enum class State
		{
			Invalid,

			WaitingForAccept,
			Accepted,
		};

		TlsServerData(const std::shared_ptr<TlsContext> &tls_context, bool is_blocking);

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

		// If a server name is provided by SNI, it should be stored in ov::TlsServerData, which is the method used at this time.
		void SetServerName(const char *server_name)
		{
			_server_name = server_name;
		}

		ov::String GetServerName() const
		{
			return _server_name;
		}

		const Tls &GetTls() const
		{
			return _tls;
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
		// Tls.Read() -> SSL_read() -> Tls::TlsRead() -> TlsBioCallback.read_callback -> TlsServerData.OnTlsRead()
		ssize_t OnTlsRead(Tls *tls, void *buffer, size_t length);
		// Tls.Write() -> SSL_write() -> Tls::TlsWrite() -> TlsBioCallback.write_callback -> TlsServerData.OnTlsWrite()
		ssize_t OnTlsWrite(Tls *tls, const void *data, size_t length);
		// OpenSSL -> Tls::() -> Tls::TlsCtrl() -> TlsBioCallback.ctrl_callback -> TlsServerData.OnTlsCtrl()
		long OnTlsCtrl(ov::Tls *tls, int cmd, long num, void *arg);

	protected:
		State _state = State::Invalid;

		ov::String _server_name;

		Tls _tls;
		WriteCallback _write_callback;

		std::mutex _cipher_data_mutex;
		std::shared_ptr<Data> _cipher_data;

		std::mutex _plain_data_mutex;
		std::shared_ptr<Data> _plain_data;
	};
}  // namespace ov
