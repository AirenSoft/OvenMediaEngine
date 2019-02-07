//==============================================================================
//
//  OvenMediaEngine
//
//  Created by benjamin
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <csignal>

#include "./string.h"

#pragma once

namespace ov
{
	class StackTrace
	{
	public:
		StackTrace() = delete;

		static void InitializeStackTrace(const char *version);
		static void ShowTrace();

	private:
		struct ParseResult
		{
			char *module_name = nullptr;
			char *address = nullptr;
			char *function_name = nullptr;
			char *offset = nullptr;
		};

		static void AbortHandler(int signum, siginfo_t *si, void *unused);
		static void PrintStackTrace(int signum, String sig_name);

		static bool ParseLinuxStyleLine(char *line, ParseResult *parse_result);
		static bool ParseMacOsStyleLine(char *line, ParseResult *parse_result);

	};
}