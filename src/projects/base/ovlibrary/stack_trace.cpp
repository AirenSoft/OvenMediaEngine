//==============================================================================
//
//  OvenMediaEngine
//
//  Created by benjamin
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <signal.h>
#include <stdlib.h>
#include <execinfo.h>
#include <errno.h>
#include <cxxabi.h>

#include "log.h"
#include "stack_trace.h"

#define OV_LOG_TAG "StackTrace"

namespace ov
{
	void StackTrace::InitializeStackTrace()
	{
		/*
 1) SIGHUP	 2) SIGINT	 3) SIGQUIT	 4) SIGILL	 5) SIGTRAP
 6) SIGABRT	 7) SIGBUS	 8) SIGFPE	 9) SIGKILL	10) SIGUSR1
11) SIGSEGV	12) SIGUSR2	13) SIGPIPE	14) SIGALRM	15) SIGTERM
16) SIGSTKFLT	17) SIGCHLD	18) SIGCONT	19) SIGSTOP	20) SIGTSTP
21) SIGTTIN	22) SIGTTOU	23) SIGURG	24) SIGXCPU	25) SIGXFSZ
26) SIGVTALRM	27) SIGPROF	28) SIGWINCH	29) SIGIO	30) SIGPWR
31) SIGSYS	34) SIGRTMIN	35) SIGRTMIN+1	36) SIGRTMIN+2	37) SIGRTMIN+3
38) SIGRTMIN+4	39) SIGRTMIN+5	40) SIGRTMIN+6	41) SIGRTMIN+7	42) SIGRTMIN+8
43) SIGRTMIN+9	44) SIGRTMIN+10	45) SIGRTMIN+11	46) SIGRTMIN+12	47) SIGRTMIN+13
48) SIGRTMIN+14	49) SIGRTMIN+15	50) SIGRTMAX-14	51) SIGRTMAX-13	52) SIGRTMAX-12
53) SIGRTMAX-11	54) SIGRTMAX-10	55) SIGRTMAX-9	56) SIGRTMAX-8	57) SIGRTMAX-7
58) SIGRTMAX-6	59) SIGRTMAX-5	60) SIGRTMAX-4	61) SIGRTMAX-3	62) SIGRTMAX-2
63) SIGRTMAX-1	64) SIGRTMAX
		*/

		struct sigaction sa;
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = AbortHandler;
		sigemptyset( &sa.sa_mask );

		sigaction( SIGABRT, &sa, nullptr );    // assert()
		sigaction( SIGSEGV, &sa, nullptr );    // illegal memory access
		sigaction( SIGBUS,  &sa, nullptr );    // illegal memory access
		sigaction( SIGILL,  &sa, nullptr );    // execute a malformed instruction.
		sigaction( SIGFPE,  &sa, nullptr );    // divide by zero
	}

	void StackTrace::AbortHandler(int signum, siginfo_t* si, void* unused)
	{
		logtc("Caught signal (%d)", signum);
		PrintStackTrace();

		exit(signum);
	}

	void StackTrace::PrintStackTrace()
	{
		void* addr_list[16];

		int buffer_size = backtrace(addr_list, sizeof(addr_list) / sizeof(void*));
		if (buffer_size == 0) return;

		char** symbol_list = backtrace_symbols(addr_list, buffer_size);

		// (0)PrintStackTrace, (1)AbortHandler
		for (int i = 2; i < buffer_size; ++i)
		{
			char* begin_name   = nullptr;
			char* begin_offset = nullptr;
			char* end_offset   = nullptr;

			for (char *p = symbol_list[i]; *p; ++p)
			{
				if ( *p == '(' )
					begin_name = p;
				else if ( *p == '+' )
					begin_offset = p;
				else if ( *p == ')' && ( begin_offset || begin_name ))
					end_offset = p;
			}

			if (begin_name && end_offset)
			{
				*begin_name++   = '\0';
				*end_offset++   = '\0';
				if ( begin_offset )
					*begin_offset++ = '\0';

				int status = 0;
				char* real_name = abi::__cxa_demangle( begin_name, nullptr, nullptr, &status );
				if (status == 0)
				{
					logtc("%s (%s + %s) %s", symbol_list[i], real_name, begin_offset, end_offset);
				}
				free(real_name);
			} else {
				logtc("%s", symbol_list[i]);
			}
		}
		free(symbol_list);
	}
}