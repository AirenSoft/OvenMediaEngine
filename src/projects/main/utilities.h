//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

struct ParseOption
{
	bool help = false;
	bool version = false;

	// -c <config_path>
	ov::String config_path = "";

	// -s start with systemctl
	bool start_service = false;
};

bool TryParseOption(int argc, char *argv[], ParseOption *parse_option);
