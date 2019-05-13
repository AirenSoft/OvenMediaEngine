//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <cstdint>
#include <functional>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/tls1.h>
#include <openssl/x509v3.h>
#include <openssl/dtls1.h>
#include <openssl/ssl.h>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovcrypto/ovcrypto.h>

namespace ov
{
	class Tls;

	struct TlsCallback
	{
		// Setting up TLS extensions, etc
		std::function<bool(Tls *tls, SSL_CTX *context)> create_callback = nullptr;

		// Return value: read bytes
		std::function<ssize_t(Tls *tls, void *buffer, size_t length)> read_callback = nullptr;

		// Return value: written bytes
		std::function<ssize_t(Tls *tls, const void *data, size_t length)> write_callback = nullptr;

		std::function<bool(Tls *tls)> destroy_callback = nullptr;

		std::function<long(Tls *tls, int cmd, long num, void *ptr)> ctrl_callback = nullptr;

		// Return value: verfied: true, not verified: false
		std::function<bool(Tls *tls, X509_STORE_CTX *store_context)> verify_callback = nullptr;
	};

	// TlsUniquePtr is like the unique_ptr, it was created to support custom deleter which has return values
	template<typename Ttype, typename Treturn, Treturn (*delete_function)(Ttype *argument)>
	class TlsUniquePtr
	{
	public:
		TlsUniquePtr()
			: _ptr(nullptr, nullptr)
		{
		}

		TlsUniquePtr(Ttype *pointer) // NOLINT
			: _ptr(pointer, Deleter)
		{
		}

		TlsUniquePtr(TlsUniquePtr &&ptr) noexcept
			: _ptr(nullptr, nullptr)
		{
			std::swap(ptr._ptr, _ptr);
		}

		operator Ttype *() noexcept // NOLINT
		{
			return _ptr.get();
		}

		operator const Ttype *() const noexcept // NOLINT
		{
			return _ptr.get();
		}

		TlsUniquePtr &operator =(TlsUniquePtr &&instance) noexcept
		{
			std::swap(_ptr, instance._ptr);

			return *this;
		}

		TlsUniquePtr &operator =(Ttype *pointer)
		{
			_ptr = std::unique_ptr<Ttype, decltype(&Deleter)>(pointer, Deleter);

			return *this;
		}

		bool operator ==(const Ttype *pointer) const
		{
			return _ptr.get() == pointer;
		}

		bool operator !=(const Ttype *pointer) const
		{
			return !(operator ==(pointer));
		}

	private:
		static void Deleter(Ttype *variable)
		{
			delete_function(variable);
		}

		std::unique_ptr<Ttype, decltype(&Deleter)> _ptr;
	};

	class Tls
	{
	public:
		Tls() = default;
		virtual ~Tls();

		// method: DTLS_server_method(), TLS_server_method()
		bool Initialize(const SSL_METHOD *method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const ov::String &cipher_list, TlsCallback callback);

		// @return Returns SSL_ERROR_NONE on success
		int Accept();

		// @return Returns SSL_ERROR_NONE on success
		int Read(void *buffer, size_t length, size_t *read_bytes);

		std::shared_ptr<ov::Data> Read();

		// @return Returns SSL_ERROR_NONE on success
		int Write(const void *data, size_t length, size_t *written_bytes);

		bool FlushInput();

		// @return The number of buffered and processed application data bytes that are pending and are available for immediate read
		int Pending() const;

		// @return SSL_has_pending() returns 1 if there is buffered record data in the SSL object and 0 otherwise.
		bool HasPending() const;

		void SetVerify(int flags);

		std::shared_ptr<Certificate> GetPeerCertificate() const;
		bool ExportKeyingMaterial(unsigned long crypto_suite, const ov::String &label, std::shared_ptr<ov::Data> &server_key, std::shared_ptr<ov::Data> &client_key);

		// APIs related to SRTP
		unsigned long GetSelectedSrtpProfileId();

		bool GetKeySaltLen(unsigned long crypto_suite, size_t *key_len, size_t *salt_len) const;

	protected:
		static BIO_METHOD *PrepareBioMethod();

		bool PrepareSslContext(const SSL_METHOD *method, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate, const ov::String &cipher_list);
		bool PrepareBio();
		bool PrepareSsl(void *app_data);

		int GetError(int code);

		template<typename Treturn, Treturn default_value, class Tmember, Tmember member, typename ... Targuments>
		static Treturn DoCallback(void *obj, Targuments ... args)
		{
			if(obj == nullptr)
			{
				OV_ASSERT2(false);
				return default_value;
			}

			auto tls = static_cast<Tls *>(obj);
			auto &target = tls->_callback.*member;

			if(target != nullptr)
			{
				return target(tls, args ...);
			}

			return default_value;
		}

		static int TlsVerify(X509_STORE_CTX *store, void *arg);

		static int TlsCreate(BIO *b);
		static long TlsCtrl(BIO *b, int cmd, long num, void *ptr);
		static int TlsRead(BIO *b, char *out, int outl);
		static int TlsWrite(BIO *b, const char *in, int inl);
		static int TlsPuts(BIO *b, const char *str);
		static int TlsDestroy(BIO *b);

	protected:
		TlsUniquePtr<SSL, void, ::SSL_free> _ssl = nullptr;
		TlsUniquePtr<SSL_CTX, void, ::SSL_CTX_free> _ssl_ctx = nullptr;
		TlsUniquePtr<BIO, int, ::BIO_free> _bio = nullptr;

		TlsCallback _callback;
	};
}
