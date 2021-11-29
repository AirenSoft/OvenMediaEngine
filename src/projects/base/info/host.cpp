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
	Host::Host(const ov::String &server_name, const ov::String &server_id, const cfg::vhost::VirtualHost &host_info)
		: cfg::vhost::VirtualHost(host_info)
	{
		_server_name = server_name;
		_server_id = server_id;
		_host_id = ov::Random::GenerateUInt32();

		std::vector<ov::String> name_list;

		for (auto name : host_info.GetHost().GetNameList())
		{
			name_list.push_back(name.CStr());
		}

		const cfg::cmn::Tls &tls_config = GetHost().GetTls();
		std::shared_ptr<ov::Error> error = nullptr;

		_certificate = Certificate::CreateCertificate(host_info.GetName(), name_list, tls_config);
	}

	ov::String Host::GetUUID() const
	{
		return ov::String::FormatString("%s/%s", _server_id.CStr(), GetName().CStr());
	}

}  // namespace info
