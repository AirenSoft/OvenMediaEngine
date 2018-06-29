#pragma once

#include "base/common_types.h"
#include <base/ovlibrary/ovlibrary.h>
#include <openssl/ec.h>
#include <openssl/ssl.h>


#define	CERT_NAME		"OME"

static const int 		DefaultCertificateLifetimeInSeconds = 60 * 60 * 24 * 30 * 12;  // 1 years
static const int 		CertificateWindowInSeconds = -60 * 60 * 24;

enum KeyType { KT_RSA, KT_ECDSA, KT_LAST, KT_DEFAULT = KT_ECDSA };

class Certificate
{
public:
	Certificate();
	Certificate(X509 *x509);
	~Certificate();

	bool 			Generate();
	bool			GenerateFromPem(ov::String filename);
	X509*			GetX509();
	EVP_PKEY*		GetPkey();
	ov::String 		GetFingerprint(ov::String algorithm);

	// Print Cert for Test
	void 			Print();
private:
	// Make ECDSA Key
	EVP_PKEY*		MakeKey();

	// Make Self-Signed Certificate
	X509* 			MakeCertificate(EVP_PKEY* pkey);
	// Make Digest
	bool			ComputeDigest(const ov::String algorithm);

	bool 			GetDigestEVP(const ov::String& algorithm, const EVP_MD** mdp);

	X509*							_X509;
	EVP_PKEY*						_pkey;
	std::shared_ptr<ov::Data>		_digest;
	ov::String						_digest_algorithm;
};
