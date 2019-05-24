//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "tls.h"

#include <utility>

#define OV_LOG_TAG "OpenSSL"

#define DO_CALLBACK_IF_AVAILBLE(return_type, default_value, object, callback_name, ...) \
    Tls::DoCallback<return_type, default_value, decltype(&TlsCallback::callback_name), &TlsCallback::callback_name>(object, ## __VA_ARGS__)

namespace ov
{
	Tls::~Tls()
	{
		_bio = nullptr;
		_ssl = nullptr;
		_ssl_ctx = nullptr;
	}

	BIO_METHOD *Tls::PrepareBioMethod()
	{
		static BIO_METHOD *bio_methods = nullptr;

		if(bio_methods == nullptr)
		{
			bio_methods = ::BIO_meth_new(BIO_TYPE_MEM, "ov::Tls");

			int result = 1;

			result = result && ::BIO_meth_set_create(bio_methods, TlsCreate);
			result = result && ::BIO_meth_set_ctrl(bio_methods, TlsCtrl);
			result = result && ::BIO_meth_set_read(bio_methods, TlsRead);
			result = result && ::BIO_meth_set_write(bio_methods, TlsWrite);
			result = result && ::BIO_meth_set_puts(bio_methods, TlsPuts);
			result = result && ::BIO_meth_set_destroy(bio_methods, TlsDestroy);

			if(result == false)
			{
				::BIO_meth_free(bio_methods);
				bio_methods = nullptr;
			}
		}

		OV_ASSERT2(bio_methods != nullptr);

		return bio_methods;
	}

	bool Tls::Initialize(const SSL_METHOD *method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const ov::String &cipher_list, TlsCallback callback)
	{
		bool result = true;

		_callback = std::move(callback);

		// Create SSL_CTX
		result = result && PrepareSslContext(method, certificate, chain_certificate, cipher_list);
		// Create BIO
		result = result && PrepareBio();
		// Create SSL
		result = result && PrepareSsl(nullptr);

		if(result == false)
		{
			_callback = TlsCallback();
		}

		return result;
	};

	bool Tls::PrepareSslContext(const SSL_METHOD *method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const ov::String &cipher_list)
	{
		do
		{
			OV_ASSERT2(_ssl_ctx == nullptr);

			// Create a new SSL session
			decltype(_ssl_ctx) ctx(::SSL_CTX_new(method));

			if(ctx == nullptr)
			{
				logte("Cannot create SSL context");
				break;
			}

			if(::SSL_CTX_use_certificate(ctx, certificate->GetX509()) != 1)
			{
				logte("Cannot use certficate: %s", ov::Error::CreateErrorFromOpenSsl()->ToString().CStr());
				break;
			}

			if((chain_certificate != nullptr) && (::SSL_CTX_add1_chain_cert(ctx, chain_certificate->GetX509()) != 1))
			{
				logte("Cannot use chain certificate: %s", ov::Error::CreateErrorFromOpenSsl()->ToString().CStr());

				break;
			}

			if(::SSL_CTX_use_PrivateKey(ctx, certificate->GetPkey()) != 1)
			{
				logte("Cannot use private key: %s", ov::Error::CreateErrorFromOpenSsl()->ToString().CStr());
				break;
			}

			// Register peer certificate verification callback
			if(_callback.verify_callback != nullptr)
			{
				::SSL_CTX_set_cert_verify_callback(ctx, TlsVerify, this);
			}
			else
			{
				// Use default
			}

			// https://curl.haxx.se/docs/ssl-ciphers.html
			// https://wiki.mozilla.org/Security/Server_Side_TLS
			::SSL_CTX_set_cipher_list(ctx, cipher_list.CStr());

			_ssl_ctx = std::move(ctx);

			bool result = DO_CALLBACK_IF_AVAILBLE(bool, false, this, create_callback, static_cast<SSL_CTX *>(_ssl_ctx));

			if(result == false)
			{
				logte("An error occurred inside create callback");

				_ssl_ctx = nullptr;

				break;
			}
		} while(false);

		return (_ssl_ctx != nullptr);
	}

	bool Tls::PrepareBio()
	{
		BIO_METHOD *bio_method = PrepareBioMethod();

		_bio = ::BIO_new(bio_method);

		if(_bio == nullptr)
		{
			OV_ASSERT2(false);
			return false;
		}

		::BIO_set_data(_bio, this);

		return true;
	}

	int Tls::TlsVerify(X509_STORE_CTX *store, void *arg)
	{
		bool result = DO_CALLBACK_IF_AVAILBLE(bool, false, arg, verify_callback, store);

		return result ? 1 : 0;
	}

	int Tls::TlsCreate(BIO *b)
	{
		::BIO_set_shutdown(b, 0);
		::BIO_set_init(b, 1);
		::BIO_set_data(b, nullptr);
		::BIO_set_flags(b, 0);

		return 1;
	}

	long Tls::TlsCtrl(BIO *b, int cmd, long num, void *ptr)
	{
		return DO_CALLBACK_IF_AVAILBLE(long, 0, BIO_get_data(b), ctrl_callback, cmd, num, ptr);
	}

	int Tls::TlsRead(BIO *b, char *out, int outl)
	{
		BIO_clear_retry_flags(b);

		OV_ASSERT2(out != nullptr);
		OV_ASSERT2(outl >= 0);

		auto read_bytes = DO_CALLBACK_IF_AVAILBLE(ssize_t, -1, BIO_get_data(b), read_callback, out, static_cast<size_t>(outl));

		if(read_bytes > 0)
		{
			return static_cast<int>(read_bytes);
		}
		else if(read_bytes == 0)
		{
			// No data to read
			BIO_set_retry_read(b);
		}
		else
		{
			// Error
		}

		return -1;
	}

	int Tls::TlsWrite(BIO *b, const char *in, int inl)
	{
		BIO_clear_retry_flags(b);

		OV_ASSERT2(in != nullptr);
		OV_ASSERT2(inl >= 0);

		auto written_bytes = DO_CALLBACK_IF_AVAILBLE(ssize_t, -1, BIO_get_data(b), write_callback, in, static_cast<size_t>(inl));

		if(written_bytes > 0)
		{
			return static_cast<int>(written_bytes);
		}
		else if(written_bytes == 0)
		{
			// No data to write
			BIO_set_retry_write(b);
		}
		else
		{
			// Error
		}

		return written_bytes;
	}

	int Tls::TlsPuts(BIO *b, const char *str)
	{
		return TlsWrite(b, str, static_cast<int>(::strlen(str)));
	}

	int Tls::TlsDestroy(BIO *b)
	{
		return DO_CALLBACK_IF_AVAILBLE(bool, false, BIO_get_data(b), destroy_callback) ? 1 : 0;
	}

	bool Tls::PrepareSsl(void *app_data)
	{
		OV_ASSERT2(_ssl_ctx != nullptr);
		OV_ASSERT2(_bio != nullptr);

		// SSL 세션 생성
		decltype(_ssl) ssl(::SSL_new(_ssl_ctx));

		if(ssl == nullptr)
		{
			return false;
		}

		// 세션 설정
		SSL_set_app_data(ssl, app_data);
		::SSL_set_bio(ssl, _bio, _bio);
		::SSL_set_read_ahead(ssl, 1);
		::SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

		EC_KEY *ecdh = ::EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

		if(ecdh == nullptr)
		{
			return false;
		}

		// To prevent double free (_bio will be freed when calling SSL_free())
		::BIO_up_ref(_bio);

		::SSL_set_options(ssl, SSL_OP_SINGLE_ECDH_USE);
		::SSL_set_tmp_ecdh(ssl, ecdh);
		::EC_KEY_free(ecdh);

		_ssl = std::move(ssl);

		return true;
	}

	int Tls::GetError(int code)
	{
		OV_ASSERT2(_ssl != nullptr);

		return ::SSL_get_error(_ssl, code);
	}

	int Tls::Accept()
	{
	    if(_ssl == nullptr)
        {
            logte("Ssl is null");
            return -1;
        }

		int result = ::SSL_accept(_ssl);

		switch(result)
		{
			case 1:
				// The TLS/SSL handshake was successfully completed, a TLS/SSL connection has been established.
				return SSL_ERROR_NONE;

			case 0:
				// The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.
				// Call SSL_get_error() with the return value ret to find out the reason.
				break;

			default:
				// The TLS/SSL handshake was not successful because a fatal error occurred either at the protocol level or a connection failure occurred.
				// The shutdown was not clean.
				// It can also occur of action is need to continue the operation for non-blocking BIOs.
				// Call SSL_get_error() with the return value ret to find out the reason.
				break;
		}

		int error = GetError(result);

		switch(error)
		{
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				// The operation did not complete; the same TLS/SSL I/O function should be called again later.
				break;

			default:
				// Another error
				logte("An error occurred while accept SSL connection: %s", ov::Error::CreateErrorFromOpenSsl()->ToString().CStr());
				// OV_ASSERT2(false);
				break;
		}

		return error;
	}

	int Tls::Read(void *buffer, size_t length, size_t *read_bytes)
	{
		OV_ASSERT2(_ssl != nullptr);

		int result = ::SSL_read(_ssl, buffer, static_cast<int>(length));

		if(result > 0)
		{
			// The read operation was successful.
			// The return value is the number of bytes actually read from the TLS/SSL connection.
			if(read_bytes != nullptr)
			{
				*read_bytes = static_cast<size_t>(result);
			}

			return SSL_ERROR_NONE;
		}

		// The read operation was not successful, because either the connection was closed,
		// an error occurred or action must be taken by the calling process.
		// Call SSL_get_error(3) with the return value ret to find out the reason.
		return GetError(result);
	}

	std::shared_ptr<ov::Data> Tls::Read()
	{
		auto data = std::make_shared<ov::Data>();

		unsigned char buf[1024];

		while(true)
		{
			size_t read_bytes = 0;

			int error = Read(buf, OV_COUNTOF(buf), &read_bytes);

			switch(error)
			{
				case SSL_ERROR_NONE:
					// Read successfully
					break;

					// Not enough data
				case SSL_ERROR_WANT_READ:
					break;

				default:
					// Another error occurred
					OV_ASSERT2(read_bytes == 0);
					return std::move(data);
			}

			if(data->Append(buf, read_bytes) == false)
			{
				return nullptr;
			}

			if(error == SSL_ERROR_WANT_READ)
			{
				return data;
			}
		}
	}

	int Tls::Write(const void *data, size_t length, size_t *written_bytes)
	{
		OV_ASSERT2(_ssl != nullptr);

		size_t write_size = 0;

        do
        {
            // max 16384(2^14)byte write
            int result = ::SSL_write(_ssl, (void *)((char *)data + write_size), static_cast<int>(length - write_size));

            if(result <= 0)
            {
                // The write operation was not successful, because either the connection was closed,
                // an error occurred or action must be taken by the calling process.
                // Call SSL_get_error() with the return value ret to find out the reason.
                return GetError(result);
            }

            write_size +=  static_cast<size_t>(result);
        }while(write_size < length);

        // The write operation was successful,
        // the return value is the number of bytes actually written to the TLS/SSL connection.
        if (written_bytes != nullptr)
		{
        	*written_bytes += write_size;
		}

		return SSL_ERROR_NONE;
	}

	bool Tls::FlushInput()
	{
		unsigned char buf[1024];

		while(HasPending())
		{
			size_t read_bytes;

			int error = Read(buf, OV_COUNTOF(buf), &read_bytes);

			if(error != SSL_ERROR_NONE)
			{
				OV_ASSERT2(false);
				return false;
			}
		}

		return true;
	}

	int Tls::Pending() const
	{
		OV_ASSERT2(_ssl != nullptr);

		// SSL_pending() returns the number of buffered and processed application data bytes that
		// are pending and are available for immediate read.
		return ::SSL_pending(_ssl);
	}

	bool Tls::HasPending() const
	{
		OV_ASSERT2(_ssl != nullptr);

		// SSL_has_pending() returns 1 if there is buffered record data in the SSL object and 0 otherwise.
		return (::SSL_has_pending(_ssl) == 1);
	}

	void Tls::SetVerify(int mode)
	{
		::SSL_CTX_set_verify(_ssl_ctx, mode, nullptr);
	}

	std::shared_ptr<Certificate> Tls::GetPeerCertificate() const
	{
		OV_ASSERT2(_ssl != nullptr);

		TlsUniquePtr<X509, void, ::X509_free> cert(::SSL_get_peer_certificate(_ssl));

		if(cert == nullptr)
		{
			logte("An error occurred while get peer certificate");
			return nullptr;
		}

		return std::make_shared<Certificate>(cert);
	}

	bool Tls::ExportKeyingMaterial(unsigned long crypto_suite, const ov::String &label, std::shared_ptr<ov::Data> &server_key, std::shared_ptr<ov::Data> &client_key)
	{
		OV_ASSERT2(_ssl != nullptr);

		size_t key_len;
		size_t salt_len;

		if(GetKeySaltLen(crypto_suite, &key_len, &salt_len) == false)
		{
			return false;
		}

		size_t capacity = (key_len + salt_len) * 2;

		auto key_data = std::make_shared<ov::Data>(capacity);
		key_data->SetLength(capacity);

		auto key_buffer = key_data->GetWritableDataAs<uint8_t>();

		int result = ::SSL_export_keying_material(
			_ssl,
			key_buffer, key_data->GetLength(),
			label.CStr(), label.GetLength(),
			nullptr, 0, false
		);

		if(result != 1)
		{
			// SSL_export_keying_material() returns 0 or -1 on failure or 1 on success.
			return false;
		}

		client_key->Clear();
		server_key->Clear();

		client_key->Append(key_buffer, key_len);
		key_buffer += key_len;

		server_key->Append(key_buffer, key_len);
		key_buffer += key_len;

		client_key->Append(key_buffer, salt_len);
		key_buffer += salt_len;

		server_key->Append(key_buffer, salt_len);

		return true;
	}

	unsigned long Tls::GetSelectedSrtpProfileId()
	{
		OV_ASSERT2(_ssl != nullptr);

		SRTP_PROTECTION_PROFILE *srtp_protection_profile = ::SSL_get_selected_srtp_profile(_ssl);

		if(srtp_protection_profile == nullptr)
		{
			return 0;
		}

		return srtp_protection_profile->id;
	}

	bool Tls::GetKeySaltLen(unsigned long crypto_suite, size_t *key_len, size_t *salt_len) const
	{
		switch(crypto_suite)
		{
			case SRTP_AES128_CM_SHA1_32:
			case SRTP_AES128_CM_SHA1_80:
				// SRTP_AES128_CM_HMAC_SHA1_32 and SRTP_AES128_CM_HMAC_SHA1_80 are defined
				// in RFC 5764 to use a 128 bits key and 112 bits salt for the cipher.
				*key_len = 16L;
				*salt_len = 14L;
				break;

			case SRTP_AEAD_AES_128_GCM:
				// SRTP_AEAD_AES_128_GCM is defined in RFC 7714 to use a 128 bits key and
				// a 96 bits salt for the cipher.
				*key_len = 16L;
				*salt_len = 12L;
				break;

			case SRTP_AEAD_AES_256_GCM:
				// SRTP_AEAD_AES_256_GCM is defined in RFC 7714 to use a 256 bits key and
				// a 96 bits salt for the cipher.
				*key_len = 32L;
				*salt_len = 12L;
				break;

			default:
				return false;
		}

		return true;
	}
};
