#pragma once

#include "base/common_types.h"
#include <base/ovlibrary/ovlibrary.h>
#include <openssl/ec.h>
#include <openssl/ssl.h>

#define    CERT_NAME        "OME"

static const int DefaultCertificateLifetimeInSeconds = 60 * 60 * 24 * 30 * 12;  // 1 years
static const int CertificateWindowInSeconds = -60 * 60 * 24;

enum class KeyType : int32_t
{
	Rsa,
	Ecdsa,
	Last,

	Default = static_cast<int>(Ecdsa)
};

class Certificate
{
public:
	Certificate() = default;
	explicit Certificate(X509 *x509);
	~Certificate();

	std::shared_ptr<ov::Error> Generate();
	std::shared_ptr<ov::Error> GenerateFromPem(const char *cert_filename, const char *private_key_filename);
	// If aux flag is enabled, it will process a trusted X509 certificate using an X509 structure
	std::shared_ptr<ov::Error> GenerateFromPem(const char *filename, bool aux);
	X509 *GetX509() const;
	EVP_PKEY *GetPkey() const ;
	ov::String GetFingerprint(const ov::String &algorithm);

	// Print Cert for Test
	void Print();
private:
	// Make ECDSA Key
	EVP_PKEY *MakeKey();

	// Make Self-Signed Certificate
	X509 *MakeCertificate(EVP_PKEY *pkey);
	// Make Digest
	bool ComputeDigest(const ov::String &algorithm);

	bool GetDigestEVP(const ov::String &algorithm, const EVP_MD **mdp);

	X509 *_X509 = nullptr;
	EVP_PKEY *_pkey = nullptr;

	ov::Data _digest;
	ov::String _digest_algorithm;
};
