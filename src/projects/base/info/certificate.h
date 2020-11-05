//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovcrypto/ovcrypto.h>
#include <config/config.h>

namespace info
{
	class Certificate
	{
	public:
		///
		/// @param certificate_name A name/identifier of certificate (Used for debugging purposes)
		/// @param host_name_list A host name list
		/// @param tls A information of certificate
		///
		static std::shared_ptr<Certificate> CreateCertificate(const ov::String &certificate_name, const std::vector<ov::String> &host_name_list, const cfg::cmn::Tls &tls);

		std::shared_ptr<::Certificate> GetCertificate() const
		{
			return _certificate;
		}

		std::shared_ptr<::Certificate> GetChainCertificate() const
		{
			return _chain_certificate;
		}

	protected:
		std::shared_ptr<ov::Error> PrepareCertificate(const ov::String &certificate_name, const std::vector<ov::String> &host_name_list, const cfg::cmn::Tls &tls);

		ov::String _certificate_name;
		std::vector<ov::String> _host_name_list;

		std::shared_ptr<::Certificate> _certificate;
		std::shared_ptr<::Certificate> _chain_certificate;
	};
}  // namespace info