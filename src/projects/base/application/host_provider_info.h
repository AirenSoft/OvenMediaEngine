//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host_base_info.h"

class HostProviderInfo : public HostBaseInfo
{
public:
	HostProviderInfo();
	virtual ~HostProviderInfo();

	ov::String ToString() const;
};
