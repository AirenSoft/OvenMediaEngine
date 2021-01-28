//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "utilities.h"

#include <getopt.h>

bool TryParseOption(int argc, char *argv[], ParseOption *parse_option)
{
	constexpr const char *opt_string = "hvic:d";

	while (true)
	{
		int name = ::getopt(argc, argv, opt_string);

		switch (name)
		{
			case -1:
				// end of arguments
				return true;

			case 'h':
				parse_option->help = true;
				return true;

			case 'v':
				parse_option->version = true;
				return true;

			case 'i':
				parse_option->ignore_last_config = true;
				return true;

			case 'c':
				parse_option->config_path = optarg;
				break;

			case 'd':
				// Don't use this option manually
				parse_option->start_service = true;
				break;

			default:  // '?'
				// invalid argument
				return false;
		}
	}
}
