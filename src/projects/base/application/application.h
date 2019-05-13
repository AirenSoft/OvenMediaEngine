//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/config.h>
#include <base/ovcrypto/ovcrypto.h>

#include "stream.h"

namespace info
{
	typedef uint32_t application_id_t;

	class Application : public cfg::Application
	{
	public:
		explicit Application(const cfg::Application &application);

		application_id_t GetId() const
		{
			return _application_id;
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

		application_id_t _application_id = 0;
		std::shared_ptr<Certificate> _certificate;
		std::shared_ptr<Certificate> _chain_certificate;
	};
}