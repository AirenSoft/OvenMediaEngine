//==============================================================================
//
//  OvenMediaEngine
//
//  Created by benjamin
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <csignal>

#pragma once

namespace ov
{
	class StackTrace
	{
	public:
		StackTrace() = default;
		virtual ~StackTrace() = default;

		static void InitializeStackTrace();

	private:
		static void AbortHandler(int signum, siginfo_t* si, void* unused);
		static void PrintStackTrace();

	};
}