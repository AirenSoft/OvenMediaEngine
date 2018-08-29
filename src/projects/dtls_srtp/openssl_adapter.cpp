//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <cstring>
#include "openssl_adapter.h"
#include "dtls_transport.h"

#define OV_LOG_TAG "DTLS"

BIO_METHOD* OpenSslAdapter::_bio_methods = nullptr;

OpenSslAdapter::OpenSslAdapter()
{
}

OpenSslAdapter::~OpenSslAdapter()
{
}

SSL* OpenSslAdapter::CreateSslSession(SSL_CTX* ctx, BIO *rbio, BIO *wbio, void *app_data)
{
	// SSL 세션 생성
	SSL *ssl = SSL_new(ctx);
	if(ssl == nullptr)
	{
		return nullptr;
	}

	// 세션 설정
	SSL_set_app_data(ssl, app_data);
	SSL_set_bio(ssl, rbio, wbio);
	SSL_set_read_ahead(ssl, 1);
	SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (ecdh == nullptr)
	{
		SSL_free(ssl);
		return nullptr;
	}

	SSL_set_options(ssl, SSL_OP_SINGLE_ECDH_USE);
	SSL_set_tmp_ecdh(ssl, ecdh);
	EC_KEY_free(ecdh);

	return ssl;
}

BIO* OpenSslAdapter::CreateBio(void *user_param)
{
	BIO_METHOD* bio_method = OpenSslAdapter::GetBioMethod();
	if(bio_method == nullptr)
	{
		return nullptr;
	}

	BIO* bio = BIO_new(bio_method);

	if(bio == nullptr)
	{
		return nullptr;
	}

	// this instance를 넣고 callback에서 받아서 처리한다.
	BIO_set_data(bio, user_param);

	return bio;
}

BIO_METHOD* OpenSslAdapter::GetBioMethod()
{
	// 한번 초기화 하면 할 필요가 없다.
	if(_bio_methods != nullptr)
	{
		return _bio_methods;
	}

	int bio_type = BIO_get_new_index();
	if(bio_type == -1)
	{
		return nullptr;
	}

	_bio_methods = BIO_meth_new(bio_type, "Dtls");
	if(_bio_methods == nullptr)
	{
		return nullptr;
	}

	if(!BIO_meth_set_write(_bio_methods, StreamWrite) ||
	   !BIO_meth_set_read(_bio_methods, StreamRead) ||
	   !BIO_meth_set_puts(_bio_methods, StreamPuts) ||
	   !BIO_meth_set_ctrl(_bio_methods, StreamCtrl) ||
	   !BIO_meth_set_create(_bio_methods, StreamNew) ||
	   !BIO_meth_set_destroy(_bio_methods, StreamFree))
	{
		BIO_meth_free(_bio_methods);
		_bio_methods = nullptr;
		return nullptr;
	}

	return _bio_methods;
}

SSL_CTX* OpenSslAdapter::SetupSslContext(std::shared_ptr<Certificate> local_certificate,
										 int (*cb) (X509_STORE_CTX *, void *))
{
	// DTLS 1.2
	SSL_CTX* ctx = SSL_CTX_new(DTLS_server_method());

	if(ctx == nullptr)
	{
		return nullptr;
	}

	if(SSL_CTX_use_certificate(ctx, local_certificate->GetX509()) != 1)
	{
		logte("Configuring cert to ctx failed");
		SSL_CTX_free(ctx);
		return nullptr;
	}

	if(SSL_CTX_use_PrivateKey(ctx, local_certificate->GetPkey()) != 1)
	{
		logte("Configuring pkey to ctx failed");
		SSL_CTX_free(ctx);
		return nullptr;
	}

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);

	// Peer Certificate 를 Verify 하는 Callback 함수를 등록한다.
	SSL_CTX_set_cert_verify_callback(ctx, cb, nullptr);

	SSL_CTX_set_cipher_list(ctx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

	// 에러면 1, 성공이면 0... 미친 함수들...
	if (SSL_CTX_set_tlsext_use_srtp(ctx, "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32"))
	{
		logte("SSL_CTX_set_tlsext_use_srtp failed");
		SSL_CTX_free(ctx);
		return nullptr;
	}

	return ctx;
}

// bio methods return 1 (or at least non-zero) on success and 0 on failure.
int OpenSslAdapter::StreamNew(BIO* b)
{
	BIO_set_shutdown(b, 0);
	BIO_set_init(b, 1);
	BIO_set_data(b, nullptr);
	BIO_set_flags(b, 0);

	return 1;
}

int OpenSslAdapter::StreamFree(BIO* b)
{
	if (b == nullptr)
	{
		return 0;
	}
	return 1;
}

int OpenSslAdapter::StreamRead(BIO* b, char* out, int outl)
{
	if (out == nullptr || outl <= 0)
	{
		return 0;
	}


	// TODO: 향후 DtlsTransport의 Read, Write 기능을 Super로 빼서 Normalize 한다.
	DtlsTransport* owner = static_cast<DtlsTransport*>(BIO_get_data(b));
	if(owner == nullptr)
	{
		return 0;
	}

	BIO_clear_retry_flags(b);
	size_t read;
	int error;
	int result = owner->Read(out, outl, &read);
	if (result == 1)
	{
		return read;
	}
		// End of Stream
	else if (result == 0)
	{
		return -1;
	}
		// Block
	else if (result == -1)
	{
		BIO_set_retry_read(b);
	}

	return -1;
}

int OpenSslAdapter::StreamWrite(BIO* b, const char* in, int inl)
{
	if (in == nullptr || inl <= 0)
	{
		return 0;
	}

	DtlsTransport* owner = static_cast<DtlsTransport*>(BIO_get_data(b));
	if(owner == nullptr)
	{
		return 0;
	}

	BIO_clear_retry_flags(b);
	size_t written;
	int error;
	int result = owner->Write(in, inl, &written);
	if (result == 1)
	{
		return written;
	}
	else if (result == -1) // Block
	{
		BIO_set_retry_write(b);
	}

	return -1;
}

int OpenSslAdapter::StreamPuts(BIO* b, const char* str)
{
	return StreamWrite(b, str, strlen(str));
}

long OpenSslAdapter::StreamCtrl(BIO* b, int cmd, long num, void* ptr)
{
	switch (cmd)
	{
		case BIO_CTRL_RESET:
			return 0;
		case BIO_CTRL_WPENDING:
		case BIO_CTRL_PENDING:
			return 0;
		case BIO_CTRL_FLUSH:
			return 1;
		case BIO_CTRL_DGRAM_QUERY_MTU:
			return 1200;
		default:
			return 0;
	}
}