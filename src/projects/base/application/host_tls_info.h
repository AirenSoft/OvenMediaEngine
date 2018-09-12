//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host_base_info.h"

class HostTlsInfo : public HostBaseInfo
{
public:
	bool CheckTls() const noexcept;

	ov::String GetCertPath() const noexcept;
	void SetCertPath(ov::String cert_path);

	ov::String GetKeyPath() const noexcept;
	void SetKeyPath(ov::String key_path);

private:
	ov::String _cert_path;
	ov::String _key_path;
};
