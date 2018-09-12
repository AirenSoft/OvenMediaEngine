//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_tls_info.h"

bool HostTlsInfo::CheckTls() const noexcept
{
	return (_cert_path.IsEmpty() == false) && (_key_path.IsEmpty() == false);
}

ov::String HostTlsInfo::GetCertPath() const noexcept
{
	return _cert_path;
}

void HostTlsInfo::SetCertPath(ov::String cert_path)
{
	_cert_path = cert_path;
}

ov::String HostTlsInfo::GetKeyPath() const noexcept
{
	return _key_path;
}

void HostTlsInfo::SetKeyPath(ov::String key_path)
{
	_key_path = key_path;
}
