#include "certificate.h"

#include <openssl/core_names.h>

#include <utility>

Certificate::Certificate(X509 *x509)
{
	_X509 = x509;

	// Increments the reference count
	X509_up_ref(_X509);
}

Certificate::~Certificate()
{
	OV_SAFE_FUNC(_X509, nullptr, X509_free, );
	OV_SAFE_FUNC(_pkey, nullptr, EVP_PKEY_free, );
}

std::shared_ptr<ov::OpensslError> Certificate::GenerateFromPem(const char *cert_filename, const char *private_key_filename)
{
	OV_ASSERT2(_X509 == nullptr);
	OV_ASSERT2(_pkey == nullptr);

	if ((_X509 != nullptr) || (_pkey != nullptr))
	{
		return std::make_shared<ov::OpensslError>("Certificate is already created");
	}

	// TODO(dimiden): If a cert file contains multiple certificates, it should be processed.
	// Read cert file
	BIO *cert_bio = nullptr;
	cert_bio = BIO_new(BIO_s_file());

	if (BIO_read_filename(cert_bio, cert_filename) <= 0)
	{
		BIO_free(cert_bio);
		return std::make_shared<ov::OpensslError>();
	}

	_X509 = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
	BIO_free(cert_bio);

	if (_X509 == nullptr)
	{
		return std::make_shared<ov::OpensslError>();
	}

	// Read private key file
	BIO *pk_bio = nullptr;
	pk_bio = BIO_new(BIO_s_file());

	if (BIO_read_filename(pk_bio, private_key_filename) <= 0)
	{
		return std::make_shared<ov::OpensslError>();
	}

	_pkey = PEM_read_bio_PrivateKey(pk_bio, nullptr, nullptr, nullptr);

	BIO_free(pk_bio);

	if (_pkey == nullptr)
	{
		return std::make_shared<ov::OpensslError>();
	}

	return nullptr;
}

std::shared_ptr<ov::OpensslError> Certificate::GenerateFromPem(const char *filename, bool aux)
{
	if (_X509 != nullptr)
	{
		return std::make_shared<ov::OpensslError>("Certificate is already created");
	}

	BIO *cert_bio = nullptr;
	cert_bio = BIO_new(BIO_s_file());

	if (BIO_read_filename(cert_bio, filename) <= 0)
	{
		return std::make_shared<ov::OpensslError>();
	}

	if (aux)
	{
		_X509 = PEM_read_bio_X509_AUX(cert_bio, nullptr, nullptr, nullptr);
	}
	else
	{
		_X509 = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
	}

	BIO_free(cert_bio);

	if (_X509 == nullptr)
	{
		return std::make_shared<ov::OpensslError>();
	}

	// TODO(dimiden): Extract these codes to another function like GenerateFromPrivateKey()
	//
	//_pkey = PEM_read_bio_PrivateKey(cert_bio, nullptr, nullptr, nullptr);
	//if(_pkey == nullptr)
	//{
	//	BIO_free(cert_bio);
	//	return std::make_shared<ov::OpensslError>();
	//}
	//
	//BIO_free(cert_bio);
	//
	//// Check Key
	//EC_KEY *ec_key = EVP_PKEY_get1_EC_KEY(_pkey);
	//if(!ec_key)
	//{
	//	return std::make_shared<ov::OpensslError>();
	//}
	//
	//if(!EC_KEY_check_key(ec_key))
	//{
	//	EC_KEY_free(ec_key);
	//	return std::make_shared<ov::OpensslError>();
	//}

	return nullptr;
}

std::shared_ptr<ov::OpensslError> Certificate::Generate()
{
	if (_X509 != nullptr)
	{
		return std::make_shared<ov::OpensslError>("Certificate is already created");
	}

	EVP_PKEY *pkey = MakeKey();
	if (pkey == nullptr)
	{
		return std::make_shared<ov::OpensslError>();
	}

	X509 *x509 = MakeCertificate(pkey);
	if (x509 == nullptr)
	{
		return std::make_shared<ov::OpensslError>();
	}

	_pkey = pkey;
	_X509 = x509;

	return nullptr;
}

// Make ECDSA Key
EVP_PKEY *Certificate::MakeKey()
{
	EVP_PKEY *key = ::EVP_PKEY_new();
	if (key == nullptr)
	{
		return nullptr;
	}

	bool succeeded = false;

	OSSL_PARAM params[2];
	char str[] = SN_X9_62_prime256v1;

	params[0] = ::OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, str, 0);
	params[1] = ::OSSL_PARAM_construct_end();

	EVP_PKEY_CTX *pkey_context;

	do
	{
		pkey_context = ::EVP_PKEY_CTX_new_from_name(nullptr, "ec", nullptr);

		if (pkey_context == nullptr)
		{
			break;
		}

		if (::EVP_PKEY_keygen_init(pkey_context) <= 0)
		{
			break;
		}

		if (::EVP_PKEY_CTX_set_params(pkey_context, params) <= 0)
		{
			break;
		}

		if (::EVP_PKEY_keygen(pkey_context, &key) <= 0)
		{
			break;
		}

		succeeded = true;
	} while (false);

	if (succeeded == false)
	{
		loge("CERT", "Could not make a key: %s", ov::OpensslError().What());
	}

	::EVP_PKEY_CTX_free(pkey_context);

	return key;
}

// Make X509 Certificate
X509 *Certificate::MakeCertificate(EVP_PKEY *pkey)
{
	// Allocation
	X509 *x509 = X509_new();
	if (x509 == nullptr)
	{
		return nullptr;
	}

	// 버전 설정
	if (!X509_set_version(x509, 2L))
	{
		X509_free(x509);
		return nullptr;
	}

	// BIGNUM Allocation
	BIGNUM *serial_number = BN_new();
	if (serial_number == nullptr)
	{
		X509_free(x509);
		return nullptr;
	}

	// Allocation
	X509_NAME *name = X509_NAME_new();
	if (name == nullptr)
	{
		X509_free(x509);
		BN_free(serial_number);
		return nullptr;
	}

	// 공개키를 pkey로 설정
	if (!X509_set_pubkey(x509, pkey))
	{
		X509_free(x509);
		return nullptr;
	}

	ASN1_INTEGER *asn1_serial_number;

	// Random 값을 뽑아서 X509 Serial Number에 사용한다.
	BN_rand(serial_number, 64, 0, 0);

	// 인증서 내부의 Serial Number를 반환 (내부값으로 해제되면 안됨)
	asn1_serial_number = X509_get_serialNumber(x509);

	// 상기 pseudo_rand로 생성한 serial_number를 ANS1_INTEGER로 변환하여 x509 내부 serial number 포인터에 바로 쓴다.
	BN_to_ASN1_INTEGER(serial_number, asn1_serial_number);

	// 인증서에 정보를 추가한다.
	if (!X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8, (unsigned char *)CERT_NAME, -1, -1, 0) ||
		!X509_set_subject_name(x509, name) ||
		!X509_set_issuer_name(x509, name))
	{
		X509_free(x509);
		BN_free(serial_number);
		X509_NAME_free(name);
		return nullptr;
	}

	// 인증서 유효기간 설정
	time_t epoch_off = 0;
	time_t now = time(nullptr);

	if (!X509_time_adj(X509_get_notBefore(x509), now + CertificateWindowInSeconds, &epoch_off) ||
		!X509_time_adj(X509_get_notAfter(x509), now + DefaultCertificateLifetimeInSeconds, &epoch_off))
	{
		X509_free(x509);
		BN_free(serial_number);
		X509_NAME_free(name);
		return nullptr;
	}

	// Signing, 인증
	if (!X509_sign(x509, pkey, EVP_sha256()))
	{
		X509_free(x509);
		BN_free(serial_number);
		X509_NAME_free(name);
		return nullptr;
	}

	BN_free(serial_number);
	X509_NAME_free(name);
	return x509;
}

void Certificate::Print()
{
	BIO *temp_memory_bio = BIO_new(BIO_s_mem());

	if (!temp_memory_bio)
	{
		loge("CERT", "Failed to allocate temporary memory bio");
		return;
	}
	X509_print_ex(temp_memory_bio, _X509, XN_FLAG_SEP_CPLUS_SPC, 0);
	BIO_write(temp_memory_bio, "\0", 1);
	char *buffer;
	BIO_get_mem_data(temp_memory_bio, &buffer);
	logd("CERT", "%s", buffer);
	BIO_free(temp_memory_bio);

	logd("CERT", "Fingerprint sha-256 : %s", GetFingerprint("sha-256").CStr());

	if (_pkey != nullptr)
	{
		BIO *bio_out = BIO_new_fp(stdout, BIO_NOCLOSE);
		logd("CERT", "Public Key :");
		EVP_PKEY_print_public(bio_out, _pkey, 0, NULL);
		logd("CERT", "Private Key ::");
		EVP_PKEY_print_private(bio_out, _pkey, 0, NULL);
		BIO_free(bio_out);
	}
}

bool Certificate::GetDigestEVP(const ov::String &algorithm, const EVP_MD **mdp)
{
	const EVP_MD *md;

	if (algorithm == "md5")
	{
		md = EVP_md5();
	}
	else if (algorithm == "sha-1")
	{
		md = EVP_sha1();
	}
	else if (algorithm == "sha-224")
	{
		md = EVP_sha224();
	}
	else if (algorithm == "sha-256")
	{
		md = EVP_sha256();
	}
	else if (algorithm == "sha-384")
	{
		md = EVP_sha384();
	}
	else if (algorithm == "sha-512")
	{
		md = EVP_sha512();
	}
	else
	{
		return false;
	}

	*mdp = md;
	return true;
}

bool Certificate::ComputeDigest(const ov::String &algorithm)
{
	const EVP_MD *md;
	unsigned int n;

	if (!GetDigestEVP(algorithm, &md))
	{
		return false;
	}

	uint8_t digest[EVP_MAX_MD_SIZE];
	X509_digest(GetX509(), md, digest, &n);
	_digest.Append(digest, n);
	_digest_algorithm = algorithm;
	return true;
}

X509 *Certificate::GetX509() const
{
	return _X509;
}

EVP_PKEY *Certificate::GetPkey() const
{
	return _pkey;
}

ov::String Certificate::GetFingerprint(const ov::String &algorithm)
{
	if (_digest.GetLength() <= 0)
	{
		if (!ComputeDigest(algorithm))
		{
			return "";
		}
	}

	ov::String fingerprint = ov::ToHexStringWithDelimiter(&_digest, ':');
	fingerprint.MakeUpper();
	return fingerprint;
}
