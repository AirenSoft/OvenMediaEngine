//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./certificate.h"

class CertificatePair
{
public:
	CertificatePair(const std::shared_ptr<const Certificate> &certificate, const std::shared_ptr<const Certificate> &chain_certificate)
		: _certificate(certificate),
		  _chain_certificate(chain_certificate)
	{
	}

	static std::shared_ptr<CertificatePair> CreateCertificatePair(const std::shared_ptr<const Certificate> &certificate, const std::shared_ptr<const Certificate> &chain_certificate)
	{
		return std::make_shared<CertificatePair>(certificate, chain_certificate);
	}

	static std::shared_ptr<CertificatePair> CreateCertificatePair(const std::shared_ptr<const Certificate> &certificate)
	{
		return CreateCertificatePair(certificate, nullptr);
	}

	std::shared_ptr<const Certificate> GetCertificate() const
	{
		return _certificate;
	}

	std::shared_ptr<const Certificate> GetChainCertificate() const
	{
		return _chain_certificate;
	}

protected:
	std::shared_ptr<const Certificate> _certificate;
	std::shared_ptr<const Certificate> _chain_certificate;
};
