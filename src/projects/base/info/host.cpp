//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host.h"

#include "host_private.h"

namespace info
{
	Host::Host(const cfg::vhost::VirtualHost &host_info)
		: cfg::vhost::VirtualHost(host_info)
	{
		_host_id = ov::Random::GenerateUInt32();

		std::vector<ov::String> name_list;

		for (auto name : host_info.GetHost().GetNameList())
		{
			name_list.push_back(name.CStr());
		}

		const cfg::cmn::Tls &tls = GetHost().GetTls();
		std::shared_ptr<ov::Error> error = nullptr;

		_certificate = Certificate::CreateCertificate(host_info.GetName(), name_list, tls);
	}
}  // namespace info
