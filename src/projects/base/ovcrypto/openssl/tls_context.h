//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../certificate_pair.h"
#include "./openssl_error.h"
#include "./tls_context_callback.h"

namespace ov
{
	enum class TlsMethod
	{
		// DTLS_server_method()
		DTls,
		// TLS_server_method() /
		Tls
	};

	// A wrapper of SSL_CTX
	class TlsContext
	{
	public:
		~TlsContext();

		static std::shared_ptr<TlsContext> CreateServerContext(
			TlsMethod method,
			const std::shared_ptr<const CertificatePair> &certificate_pair,
			const ov::String &cipher_list,
			const ov::TlsContextCallback *callback,
			// output param
			std::shared_ptr<const ov::Error> *error);

		static std::shared_ptr<TlsContext> CreateClientContext(
			// output param
			std::shared_ptr<const ov::Error> *error);

		const SSL_CTX *GetSslContext() const noexcept
		{
			return _ssl_ctx;
		}

		SSL_CTX *GetSslContext() noexcept
		{
			return _ssl_ctx;
		}

		bool UseSslContext(SSL *ssl);

		void SetVerify(int mode);

	protected:
		std::shared_ptr<ov::Error> Prepare(
			const SSL_METHOD *method,
			const std::shared_ptr<const CertificatePair> &certificate_pair,
			const ov::String &cipher_list,
			const TlsContextCallback *callback);

		std::shared_ptr<ov::Error> Prepare(
			const SSL_METHOD *method,
			const TlsContextCallback *callback);

		template <typename Treturn, Treturn default_value, class Tmember, Tmember member, typename... Targuments>
		static Treturn DoCallback(void *tls_context, Targuments... args)
		{
			if (tls_context == nullptr)
			{
				OV_ASSERT2(false);
				return default_value;
			}

			auto tls = static_cast<TlsContext *>(tls_context);
			auto &target = tls->_callback.*member;

			if (target != nullptr)
			{
				return target(tls, args...);
			}

			return default_value;
		}

		static int OnServerNameCallback(SSL *s, int *ad, void *arg);
		int OnServerName(SSL *ssl);

		std::shared_ptr<ov::Error> SetCertificate(const std::shared_ptr<const CertificatePair> &certificate_pair);

		static int TlsVerify(X509_STORE_CTX *store, void *arg);

	protected:
		SSL_CTX *_ssl_ctx = nullptr;

		TlsContextCallback _callback;
	};
}  // namespace ov
