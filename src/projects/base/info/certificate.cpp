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
		if (tls.IsParsed() == false)
		{
			// TLS is disabled
			return nullptr;
		}

		auto certificate = std::make_shared<Certificate>();

		auto error = certificate->PrepareCertificate(certificate_name, host_name_list, tls);

		ov::String cert_path = tls.GetCertPath();
		ov::String key_path = tls.GetKeyPath();
		ov::String chain_cert_path = tls.GetChainCertPath();

		ov::String chain_description = chain_cert_path.IsEmpty() ? "N/A" : ov::String::FormatString("Chain cert file path: %s", chain_cert_path.CStr());

		if (error == nullptr)
		{
			logti(
				"A certificate has been created for VirtualHost [%s]:\n"
				"\tCert file path: %s\n"
				"\tChain cert file path: %s\n"
				"\tPrivate key file path: %s",
				certificate_name.CStr(),
				cert_path.CStr(),
				chain_cert_path.CStr(),
				key_path.CStr());
		}
		else
		{
			logte(
				"Failed to create a certificate for VirtualHost [%s]:\n"
				"\tReason: %s\n"
				"\tCert file path: %s\n"
				"\tChain cert file path: %s\n"
				"\tPrivate key file path: %s",
				certificate_name.CStr(),
				error->GetMessage().CStr(),
				cert_path.CStr(),
				chain_cert_path.CStr(),
				key_path.CStr());

			return nullptr;
		}

		return certificate;
	}

	std::shared_ptr<ov::Error> Certificate::PrepareCertificate(const ov::String &certificate_name, const std::vector<ov::String> &host_name_list, const cfg::cmn::Tls &tls)
	{
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

		auto error = _certificate->GenerateFromPem(cert_path, key_path);

		if (error != nullptr)
		{
			_certificate = nullptr;
			return ov::Error::CreateError(OV_LOG_TAG, "Could not create a certificate from file - %s", error->ToString().CStr());
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
			_chain_certificate = nullptr;
			return ov::Error::CreateError(OV_LOG_TAG, "Could not create a chain certificate from file - %s", error->ToString().CStr());
		}

		return nullptr;
	}
}  // namespace info