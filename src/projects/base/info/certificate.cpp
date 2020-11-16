//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "certificate.h"

#define OV_LOG_TAG "Certificate"

namespace info
{
	std::shared_ptr<Certificate> Certificate::CreateCertificate(const ov::String &certificate_name, const std::vector<ov::String> &host_name_list, const cfg::cmn::Tls &tls)
	{
		auto certificate = std::make_shared<Certificate>();

		auto error = certificate->PrepareCertificate(certificate_name, host_name_list, tls);

		if (error != nullptr)
		{
			logte("Could not prepare certificate for application %s: %s",
				  certificate_name.CStr(),
				  error->ToString().CStr());

			return nullptr;
		}

		return certificate;
	}

	std::shared_ptr<ov::Error> Certificate::PrepareCertificate(const ov::String &certificate_name, const std::vector<ov::String> &host_name_list, const cfg::cmn::Tls &tls)
	{
		if (tls.IsParsed() == false)
		{
			// TLS is disabled
			return nullptr;
		}

		_certificate_name = certificate_name;

		_host_name_list = host_name_list;

		ov::String cert_path = tls.GetCertPath();
		ov::String key_path = tls.GetKeyPath();
		ov::String chain_cert_path = tls.GetChainCertPath();

		if (cert_path.IsEmpty())
		{
			return ov::Error::CreateError(OV_LOG_TAG, "Invalid TLS configuration: <CertPath> must not be empty");
		}

		if (key_path.IsEmpty())
		{
			return ov::Error::CreateError(OV_LOG_TAG, "Invalid TLS configuration: <KeyPath> must not be empty");
		}

		_certificate = std::make_shared<::Certificate>();

		logti(
			"Trying to create a certificate using\n"
			"\tCert file path: %s\n"
			"%s%s%s"
			"\tPrivate key file path: %s",
			cert_path.CStr(),
			chain_cert_path.IsEmpty() ? "" : "\tChain cert file path: ",
			chain_cert_path.IsEmpty() ? "" : chain_cert_path.CStr(),
			chain_cert_path.IsEmpty() ? "" : "\n",
			key_path.CStr());

		auto error = _certificate->GenerateFromPem(cert_path, key_path);

		if (error != nullptr)
		{
			logte("Could not create a certificate from files: %s", error->ToString().CStr());
			_certificate = nullptr;
			return error;
		}

		if (chain_cert_path.IsEmpty())
		{
			// Chain certificate is optional, and it's not available
			return nullptr;
		}

		_chain_certificate = std::make_shared<::Certificate>();

		error = _chain_certificate->GenerateFromPem(chain_cert_path, true);

		if (error != nullptr)
		{
			logte("Could not create a chain certificate from file: %s", error->ToString().CStr());
			_chain_certificate = nullptr;
			return error;
		}

		return nullptr;
	}
}  // namespace info