#include "certificate.h"

Certificate::Certificate()
{
	_X509 = nullptr;
	_pkey = nullptr;
	_digest = ov::Data::CreateData();
}

Certificate::Certificate(X509 *x509)
{
	_X509 = x509;
	_pkey = nullptr;

	// Increments the reference count
	X509_up_ref(_X509);

	_digest = ov::Data::CreateData();
}

Certificate::~Certificate()
{

}

bool Certificate::GenerateFromPem(ov::String filename)
{
	if(_X509 != nullptr)
	{
		return false;
	}

	BIO *cert_bio = NULL;
	cert_bio = BIO_new(BIO_s_file());

	if(BIO_read_filename(cert_bio, filename.CStr()) <= 0)
	{
		return false;
	}

	_X509 = PEM_read_bio_X509(cert_bio, NULL, NULL, NULL);
	if(_X509 == nullptr)
	{
		BIO_free(cert_bio);
		return false;
	}

	_pkey = PEM_read_bio_PrivateKey(cert_bio, nullptr, nullptr, nullptr);
	if(_pkey == nullptr)
	{
		loge("CERT", "Read private-key failed");
		BIO_free(cert_bio);
		return false;
	}

	BIO_free(cert_bio);

	// Check Key
	EC_KEY *ec_key = EVP_PKEY_get1_EC_KEY(_pkey);
	if(!ec_key)
	{
		loge("CERT", "Get ec key from pkey failed");
		return false;
	}

	if(!EC_KEY_check_key(ec_key))
	{
		loge("CERT", "ec_key check failed");
		EC_KEY_free(ec_key);
		return false;
	}

	return true;
}

bool Certificate::Generate()
{
	if(_X509 != nullptr)
	{
		return false;
	}

	EVP_PKEY *pkey = MakeKey();
	if(pkey == nullptr)
	{
		return false;
	}

	X509 *x509 = MakeCertificate(pkey);
	if(x509 == nullptr)
	{
		return false;
	}

	_pkey = pkey;
	_X509 = x509;

	return true;
}

// Make ECDSA Key
EVP_PKEY *Certificate::MakeKey()
{
	EVP_PKEY *key;

	key = EVP_PKEY_new();
	if(key == nullptr)
	{
		return nullptr;
	}

	EC_KEY *ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if(ec_key == nullptr)
	{
		EVP_PKEY_free(key);
		return nullptr;
	}

	EC_KEY_set_asn1_flag(ec_key, OPENSSL_EC_NAMED_CURVE);

	if(!EC_KEY_generate_key(ec_key) || !EVP_PKEY_assign_EC_KEY(key, ec_key))
	{
		EC_KEY_free(ec_key);
		EVP_PKEY_free(key);
		return nullptr;
	}

	return key;
}

// Make X509 Certificate
X509 *Certificate::MakeCertificate(EVP_PKEY *pkey)
{
	// Allocation
	X509 *x509 = X509_new();
	if(x509 == nullptr)
	{
		return nullptr;
	}

	// 버전 설정
	if(!X509_set_version(x509, 2L))
	{
		X509_free(x509);
		return nullptr;
	}

	// BIGNUM Allocation
	BIGNUM *serial_number = BN_new();
	if(serial_number == nullptr)
	{
		X509_free(x509);
		return nullptr;
	}

	// Allocation
	X509_NAME *name = X509_NAME_new();
	if(name == nullptr)
	{
		X509_free(x509);
		BN_free(serial_number);
		return nullptr;
	}

	// 공개키를 pkey로 설정
	if(!X509_set_pubkey(x509, pkey))
	{
		X509_free(x509);
		return nullptr;
	}

	ASN1_INTEGER *asn1_serial_number;

	// Random 값을 뽑아서 X509 Serial Number에 사용한다.
	BN_pseudo_rand(serial_number, 64, 0, 0);

	// 인증서 내부의 Serial Number를 반환 (내부값으로 해제되면 안됨)
	asn1_serial_number = X509_get_serialNumber(x509);

	// 상기 pseudo_rand로 생성한 serial_number를 ANS1_INTEGER로 변환하여 x509 내부 serial number 포인터에 바로 쓴다.
	BN_to_ASN1_INTEGER(serial_number, asn1_serial_number);

	// 인증서에 정보를 추가한다.
	if(!X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8, (unsigned char *)CERT_NAME, -1, -1, 0) ||
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

	if(!X509_time_adj(X509_get_notBefore(x509), now + CertificateWindowInSeconds, &epoch_off) ||
	   !X509_time_adj(X509_get_notAfter(x509), now + DefaultCertificateLifetimeInSeconds, &epoch_off))
	{
		X509_free(x509);
		BN_free(serial_number);
		X509_NAME_free(name);
		return nullptr;
	}

	// Signing, 인증
	if(!X509_sign(x509, pkey, EVP_sha256()))
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

	if(!temp_memory_bio)
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

	if(_pkey != nullptr)
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

	if(algorithm == "md5")
	{
		md = EVP_md5();
	}
	else if(algorithm == "sha-1")
	{
		md = EVP_sha1();
	}
	else if(algorithm == "sha-224")
	{
		md = EVP_sha224();
	}
	else if(algorithm == "sha-256")
	{
		md = EVP_sha256();
	}
	else if(algorithm == "sha-384")
	{
		md = EVP_sha384();
	}
	else if(algorithm == "sha-512")
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

bool Certificate::ComputeDigest(const ov::String algorithm)
{
	const EVP_MD *md;
	unsigned int n;

	if(!GetDigestEVP(algorithm, &md))
	{
		return false;
	}

	uint8_t digest[EVP_MAX_MD_SIZE];
	X509_digest(GetX509(), md, digest, &n);
	_digest->Append(const_cast<uint8_t *>(digest), n);
	_digest_algorithm = algorithm;
	return true;
}

X509 *Certificate::GetX509()
{
	return _X509;
}

EVP_PKEY *Certificate::GetPkey()
{
	return _pkey;
}

//TODO(getroot): Algorithm을 enum값으로 변경
ov::String Certificate::GetFingerprint(ov::String algorithm)
{
	if(_digest->GetLength() <= 0)
	{
		if(!ComputeDigest(algorithm))
		{
			return "";
		}
	}

	ov::String fingerprint = ov::ToHexStringWithDelimiter(_digest->GetDataAs<char>(), _digest->GetLength(), ':');
	fingerprint.MakeUpper();
	return fingerprint;
}
