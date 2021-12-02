//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "tls_context.h"

#include "./openssl_private.h"
#include "./tls.h"

#define DO_CALLBACK_IF_AVAILBLE(return_type, default_value, tls_context, callback_name, ...) \
	DoCallback<return_type, default_value, decltype(&TlsContextCallback::callback_name), &TlsContextCallback::callback_name>(tls_context, ##__VA_ARGS__)

namespace ov
{
	TlsContext::~TlsContext()
	{
		OV_SAFE_FUNC(_ssl_ctx, nullptr, ::SSL_CTX_free, );

		_callback = {};
	}

	std::shared_ptr<TlsContext> TlsContext::CreateServerContext(
		TlsMethod method,
		const std::shared_ptr<const CertificatePair> &certificate_pair,
		const ov::String &cipher_list,
		const ov::TlsContextCallback *callback,
		std::shared_ptr<const ov::Error> *error)
	{
		const SSL_METHOD *ssl_method = (method == TlsMethod::Tls) ? ::TLS_server_method() : ::DTLS_server_method();

		auto context = std::make_shared<TlsContext>();

		auto prepare_error = context->Prepare(
			ssl_method,
			certificate_pair,
			cipher_list,
			callback);

		if (prepare_error != nullptr)
		{
			if (error != nullptr)
			{
				*error = prepare_error;
			}

			return nullptr;
		}

		return context;
	}

	std::shared_ptr<TlsContext> TlsContext::CreateClientContext(
		std::shared_ptr<const ov::Error> *error)
	{
		auto context = std::make_shared<TlsContext>();

		auto prepare_error = context->Prepare(
			TLS_client_method(),
			nullptr);

		if (prepare_error != nullptr)
		{
			if (error != nullptr)
			{
				*error = prepare_error;
			}

			return nullptr;
		}

		return context;
	}

	std::shared_ptr<ov::Error> TlsContext::Prepare(
		const SSL_METHOD *method,
		const std::shared_ptr<const CertificatePair> &certificate_pair,
		const ov::String &cipher_list,
		const TlsContextCallback *callback)
	{
		do
		{
			OV_ASSERT2(_ssl_ctx == nullptr);

			if (certificate_pair == nullptr)
			{
				OV_ASSERT2(certificate_pair != nullptr);

				return OpensslError::CreateError("Invalid TLS certificate");
			}

			// Create a new SSL session
			decltype(_ssl_ctx) ctx(::SSL_CTX_new(method));

			if (ctx == nullptr)
			{
				return OpensslError::CreateError("Cannot create SSL context");
			}

			if (callback != nullptr)
			{
				_callback = *callback;
			}

			// Register peer certificate verification callback
			if (_callback.verify_callback != nullptr)
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

			// Disable TLS1.3 because it is not yet supported properly by the HTTP server implementation (HTTP2 support, Session tickets, ...)
			// This also allows for using less secure cipher suites for lower CPU requirements when using HLS/DASH/LL-DASH streaming
			::SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
			// Disable old TLS versions which are neither secure nor needed any more
			::SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

			_ssl_ctx = std::move(ctx);

			bool result = DO_CALLBACK_IF_AVAILBLE(bool, true, this, create_callback, static_cast<SSL_CTX *>(ctx));

			if (result == false)
			{
				_callback = {};

				OV_SAFE_FUNC(_ssl_ctx, nullptr, ::SSL_CTX_free, );
				return OpensslError::CreateError("An error occurred inside create callback");
			}

			if (certificate_pair != nullptr)
			{
				auto error = SetCertificate(certificate_pair);

				if (error != nullptr)
				{
					_callback = {};

					OV_SAFE_FUNC(_ssl_ctx, nullptr, ::SSL_CTX_free, );
					return OpensslError::CreateError("An error occurred inside create callback");
				}
			}

			if (_callback.sni_callback != nullptr)
			{
				// Use SNI
				::SSL_CTX_set_tlsext_servername_callback(_ssl_ctx, OnServerNameCallback);
				::SSL_CTX_set_tlsext_servername_arg(_ssl_ctx, this);
			}
		} while (false);

		return nullptr;
	}

	std::shared_ptr<ov::Error> TlsContext::Prepare(
		const SSL_METHOD *method,
		const TlsContextCallback *callback)
	{
		do
		{
			OV_ASSERT2(_ssl_ctx == nullptr);

			// Create a new SSL session
			decltype(_ssl_ctx) ctx(::SSL_CTX_new(method));

			if (ctx == nullptr)
			{
				logte("Cannot create SSL context");
				break;
			}

			if (callback != nullptr)
			{
				_callback = *callback;
			}

			// Register peer certificate verification callback
			if (_callback.verify_callback != nullptr)
			{
				::SSL_CTX_set_cert_verify_callback(ctx, TlsVerify, this);
			}
			else
			{
				// Use default
			}

			_ssl_ctx = std::move(ctx);

			bool result = DO_CALLBACK_IF_AVAILBLE(bool, true, this, create_callback, static_cast<SSL_CTX *>(ctx));

			if (result == false)
			{
				_callback = {};

				OV_SAFE_FUNC(_ssl_ctx, nullptr, ::SSL_CTX_free, );
				return OpensslError::CreateError("An error occurred inside create callback");
			}
		} while (false);

		return nullptr;
	}

	int TlsContext::OnServerNameCallback(SSL *s, int *ad, void *arg)
	{
		return static_cast<TlsContext *>(arg)->OnServerName(s);
	}

	// https://www.openssl.org/docs/man1.1.1/man3/SSL_CTX_set_tlsext_servername_callback.html
	int TlsContext::OnServerName(SSL *ssl)
	{
		ov::String server_name = ::SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

		if (server_name.IsEmpty() == false)
		{
			// Client set a server name
			bool result = DO_CALLBACK_IF_AVAILBLE(bool, false, this, sni_callback, ssl, server_name);

			if (result == false)
			{
				logtw("Could not select certificate: %s", server_name.CStr());
			}
		}
		else
		{
			logtd("Server name is not specified");
		}

		return SSL_TLSEXT_ERR_OK;
	}

	std::shared_ptr<ov::Error> TlsContext::SetCertificate(const std::shared_ptr<const CertificatePair> &certificate_pair)
	{
		OV_ASSERT2(certificate_pair != nullptr);

		if (certificate_pair == nullptr)
		{
			return OpensslError::CreateError("certificate_pair is nullptr");
		}

		auto certificate = certificate_pair->GetCertificate();
		auto chain_certificate = certificate_pair->GetChainCertificate();

		if (certificate == nullptr)
		{
			return OpensslError::CreateError("certificate is nullptr");
		}

		if (::SSL_CTX_use_certificate(_ssl_ctx, certificate->GetX509()) != 1)
		{
			return OpensslError::CreateError("Cannot use certficate: %s", ov::OpensslError::CreateErrorFromOpenssl()->ToString().CStr());
		}

		if ((chain_certificate != nullptr) && (::SSL_CTX_add1_chain_cert(_ssl_ctx, chain_certificate->GetX509()) != 1))
		{
			return OpensslError::CreateError("Cannot use chain certificate: %s", ov::OpensslError::CreateErrorFromOpenssl()->ToString().CStr());
		}

		if (::SSL_CTX_use_PrivateKey(_ssl_ctx, certificate->GetPkey()) != 1)
		{
			return OpensslError::CreateError("Cannot use private key: %s", ov::OpensslError::CreateErrorFromOpenssl()->ToString().CStr());
		}

		return nullptr;
	}

	bool TlsContext::UseSslContext(SSL *ssl)
	{
		return (::SSL_set_SSL_CTX(ssl, _ssl_ctx) != nullptr);
	}

	void TlsContext::SetVerify(int mode)
	{
		::SSL_CTX_set_verify(_ssl_ctx, mode, nullptr);
	}

	int TlsContext::TlsVerify(X509_STORE_CTX *store, void *arg)
	{
		bool result = DO_CALLBACK_IF_AVAILBLE(bool, false, arg, verify_callback, store);

		return result ? 1 : 0;
	}

}  // namespace ov
