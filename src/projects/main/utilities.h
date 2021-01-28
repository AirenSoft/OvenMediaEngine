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
	// -h
	// Show help messages
	bool help = false;

	// -v
	// Show OME version
	bool version = false;

	// -i
	// Ignore CFG_LAST_CONFIG_FILE_NAME
	bool ignore_last_config = false;

	// -c <config_path>
	// Load configurations from the path
	ov::String config_path = "";

	// -d
	// Run as a service
	bool start_service = false;
};

bool TryParseOption(int argc, char *argv[], ParseOption *parse_option);
