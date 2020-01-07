//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovcrypto/ovcrypto.h>
#include <config/config.h>

#include "application.h"

namespace info
{
	typedef uint32_t host_id_t;

	class Host : public cfg::VirtualHost
	{
	public:
		explicit Host(const cfg::VirtualHost &host_info);

		host_id_t GetId() const
		{
			return _host_id;
		}

		std::shared_ptr<Certificate> GetCertificate() const
		{
			return _certificate;
		}

		std::shared_ptr<Certificate> GetChainCertificate() const
		{
			return _chain_certificate;
		}

	protected:
		std::shared_ptr<ov::Error> PrepareCertificates();

		host_id_t _host_id = 0;
		std::shared_ptr<Certificate> _certificate;
		std::shared_ptr<Certificate> _chain_certificate;
	};

}  // namespace info