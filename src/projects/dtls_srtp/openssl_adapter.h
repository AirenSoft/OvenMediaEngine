//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/tls1.h>
#include <openssl/x509v3.h>
#include <openssl/dtls1.h>
#include <openssl/ssl.h>
#include "base/ovcrypto/certificate.h"

#pragma once

class OpenSslAdapter
{
public:
	OpenSslAdapter();
	virtual ~OpenSslAdapter();

	static SSL*			CreateSslSession(SSL_CTX* ctx, BIO *rbio, BIO *wbio, void *app_data);

	static SSL_CTX*		SetupSslContext(std::shared_ptr<Certificate> local_certificate,
										   int (*cb) (X509_STORE_CTX *, void *));
	static BIO*			CreateBio(void *user_param);


private:
	static BIO_METHOD*	GetBioMethod();
	static int 			StreamWrite(BIO* h, const char* buf, int num);
	static int 			StreamRead(BIO* h, char* buf, int size);
	static int 			StreamPuts(BIO* h, const char* str);
	static long 		StreamCtrl(BIO* h, int cmd, long arg1, void* arg2);
	static int 			StreamNew(BIO* h);
	static int 			StreamFree(BIO* data);

	static BIO_METHOD*	_bio_methods;
};
