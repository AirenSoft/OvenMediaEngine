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

#include <iostream>
#include <fstream>

#include <unistd.h>

#include "stack_trace.h"

#include "platform.h"

#define SIGNAL_CASE(x) \
    case x: \
        sig_name = #x; \
        break

namespace ov
{
	String instance_version;

	void StackTrace::InitializeStackTrace(const char *version)
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

		struct sigaction sa {};

		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = AbortHandler;
		sigemptyset(&sa.sa_mask);

		/* Intentional signals (ignore)
		 * SIGQUIT, SIGINT, SIGTERM, SIGTRAP, SIGHUP, SIGKILL
		 * SIGVTALRM, SIGPROF, SIGALRM
		 */

		// Core dumped signal
		sigaction(SIGABRT, &sa, nullptr);    // assert()
		sigaction(SIGSEGV, &sa, nullptr);    // illegal memory access
		sigaction(SIGBUS, &sa, nullptr);    // illegal memory access
		sigaction(SIGILL, &sa, nullptr);    // execute a malformed instruction.
		sigaction(SIGFPE, &sa, nullptr);    // divide by zero
		sigaction(SIGSYS, &sa, nullptr);    // bad system call
		sigaction(SIGXCPU, &sa, nullptr);    // cpu time limit exceeded
		sigaction(SIGXFSZ, &sa, nullptr);    // file size limit exceeded

		// Terminated signal
		sigaction(SIGPIPE, &sa, nullptr);    // write on a pipe with no one to read it
#if IS_LINUX
		sigaction(SIGPOLL, &sa, nullptr);    // pollable event
#endif // IS_LINUX

		instance_version = version;
	}

	String StackTrace::GetStackTrace(int line_count)
	{
		return std::move(GetStackTraceInternal(2, line_count));
	}

	void StackTrace::AbortHandler(int signum, siginfo_t *si, void *unused)
	{
		String sig_name;

		switch(signum)
		{
#if IS_LINUX
			// http://man7.org/linux/man-pages/man7/signal.7.html
			// Linux (CentOS)
			SIGNAL_CASE(SIGHUP);
			SIGNAL_CASE(SIGINT);
			SIGNAL_CASE(SIGQUIT);
			SIGNAL_CASE(SIGILL);
			SIGNAL_CASE(SIGTRAP);
			SIGNAL_CASE(SIGABRT);
			SIGNAL_CASE(SIGBUS);
			SIGNAL_CASE(SIGFPE);
			SIGNAL_CASE(SIGKILL);
			SIGNAL_CASE(SIGUSR1);
			SIGNAL_CASE(SIGSEGV);
			SIGNAL_CASE(SIGUSR2);
			SIGNAL_CASE(SIGPIPE);
			SIGNAL_CASE(SIGALRM);
			SIGNAL_CASE(SIGTERM);
			SIGNAL_CASE(SIGSTKFLT);
			SIGNAL_CASE(SIGCHLD);
			SIGNAL_CASE(SIGCONT);
			SIGNAL_CASE(SIGSTOP);
			SIGNAL_CASE(SIGTSTP);
			SIGNAL_CASE(SIGTTIN);
			SIGNAL_CASE(SIGTTOU);
			SIGNAL_CASE(SIGURG);
			SIGNAL_CASE(SIGXCPU);
			SIGNAL_CASE(SIGXFSZ);
			SIGNAL_CASE(SIGVTALRM);
			SIGNAL_CASE(SIGPROF);
			SIGNAL_CASE(SIGWINCH);
			SIGNAL_CASE(SIGPOLL);
			SIGNAL_CASE(SIGPWR);
			SIGNAL_CASE(SIGSYS);
#elif IS_MACOS
			// Apple OSX and iOS (Darwin)
			SIGNAL_CASE(SIGHUP);
			SIGNAL_CASE(SIGINT);
			SIGNAL_CASE(SIGQUIT);
			SIGNAL_CASE(SIGILL);
			SIGNAL_CASE(SIGTRAP);
			SIGNAL_CASE(SIGABRT);
			SIGNAL_CASE(SIGEMT);
			SIGNAL_CASE(SIGFPE);
			SIGNAL_CASE(SIGKILL);
			SIGNAL_CASE(SIGBUS);
			SIGNAL_CASE(SIGSEGV);
			SIGNAL_CASE(SIGSYS);
			SIGNAL_CASE(SIGPIPE);
			SIGNAL_CASE(SIGALRM);
			SIGNAL_CASE(SIGTERM);
			SIGNAL_CASE(SIGURG);
			SIGNAL_CASE(SIGSTOP);
			SIGNAL_CASE(SIGTSTP);
			SIGNAL_CASE(SIGCONT);
			SIGNAL_CASE(SIGCHLD);
			SIGNAL_CASE(SIGTTIN);
			SIGNAL_CASE(SIGTTOU);
			SIGNAL_CASE(SIGIO);
			SIGNAL_CASE(SIGXCPU);
			SIGNAL_CASE(SIGXFSZ);
			SIGNAL_CASE(SIGVTALRM);
			SIGNAL_CASE(SIGPROF);
			SIGNAL_CASE(SIGWINCH);
			SIGNAL_CASE(SIGINFO);
			SIGNAL_CASE(SIGUSR1);
			SIGNAL_CASE(SIGUSR2);
#endif
			default:
				sig_name = "UNKNOWN";
		}

		WriteStackTrace(signum, sig_name);

		exit(signum);
	}

	bool StackTrace::ParseLinuxStyleLine(char *line, ParseResult *parse_result)
	{
		char *begin_name = nullptr;
		char *begin_offset = nullptr;
		char *end_offset = nullptr;
		char *begin_address = nullptr;
		char *end_address = nullptr;

		//
		// Try to parse the linux style backtrace line
		//
		// For example:
		// ./StackTraceTest(_Z16TemplateFunctionIiET_v+0x9) [0x402c87]
		//   |             |                          |   | |        |
		//  line      begin_name        begin_offset--+   | |        |
		//                                    end_offset--+ |        |
		//                                   begin_address--+        +--end_address
		//
		// Skip "./" prefix if exists
		if((line[0] == '.') && (line[1] == '/'))
		{
			line += 2;
		}

		char *current = line;

		while(*current != '\0')
		{
			switch(*current)
			{
				case '(':
					begin_name = current;
					break;

				case '+':
					if(begin_name != nullptr)
					{
						begin_offset = current;
					}
					break;

				case ')':
					if(begin_name != nullptr)
					{
						end_offset = current;
					}
					break;

				case '[':
					begin_address = current;
					break;

				case ']':
					end_address = current;
					break;

				default:
					break;
			}

			++current;
		}

		if((begin_name == nullptr) && (begin_address == nullptr))
		{
			// Does not seem to be linux-style line
			return false;
		}

		parse_result->module_name = nullptr;
		parse_result->address = nullptr;
		parse_result->function_name = nullptr;
		parse_result->offset = nullptr;

		if((begin_name != nullptr) && (end_offset != nullptr))
		{
			*begin_name++ = '\0';
			if(begin_offset != nullptr)
			{
				*begin_offset++ = '\0';
			}
			*end_offset++ = '\0';

			// Before:
			// StackTraceTest(_Z16TemplateFunctionIiET_v+0x9) [0x402c87]
			//               |                          |   |
			//          begin_name        begin_offset--+   |
			//                                  end_offset--+
			// After (! means nullptr):
			// StackTraceTest!_Z16TemplateFunctionIiET_v!0x9! [0x402c87]
			//                |                          |   |
			//            begin_name       begin_offset--+   |
			//                                   end_offset--+

			// Demangle the name
			int status = 0;
			parse_result->function_name = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);
			parse_result->offset = begin_offset;
		}

		if((begin_address != nullptr) && (end_address != nullptr))
		{
			*begin_address++ = '\0';
			*end_address++ = '\0';

			// Before:
			// StackTraceTest!_Z16TemplateFunctionIiET_v!0x9! [0x402c87]
			//                                                |        |
			//                                 begin_address--+        +--end_address
			// After (! means nullptr):
			// StackTraceTest!_Z16TemplateFunctionIiET_v!0x9! !0x402c87!
			//                                                 |        |
			//                                  begin_address--+        +--end_address
			parse_result->address = begin_address;
		}

		parse_result->module_name = line;

		return true;
	}

	bool StackTrace::ParseMacOsStyleLine(char *line, ov::StackTrace::ParseResult *parse_result)
	{
		char *begin_name = nullptr;
		char *begin_offset = nullptr;
		char *begin_address = nullptr;

		//
		// Try to parse the macOS style backtrace line
		//
		// For example:
		// 2   StackTraceTest                      0x000000010c4ae2f8 _ZN2ov10StackTrace9ShowTraceEv + 1234
		//     |                                   |                  |                                |
		//    line                  begin_address--+      begin_name--+                  begin_offset--+
		//

		// Since the module name may contain spaces, it must be parsed from the back

		int token_count = 0;
		char *current = line;

		// Trim the line

		// Move to end of the "line"
		while(*current != '\0')
		{
			current++;
		}

		while(current > line)
		{
			if((*current != ' ') && (*current != '\0'))
			{
				break;
			}

			*current-- = '\0';
		}

		// Extract "begin_address" and "begin_name" and "begin_offset" from the "line"
		while(current > line)
		{
			if(*current == ' ')
			{
				switch(token_count)
				{
					case 0:
						begin_offset = current + 1;
						break;

					case 1:
						// +
						break;

					case 2:
						begin_name = current + 1;
						break;

					case 3:
						begin_address = current + 1;

					default:
						break;
				}

				token_count++;
				*current = '\0';
			}

			if(token_count == 4)
			{
				break;
			}

			current--;
		}

		if(token_count != 4)
		{
			return false;
		}

		// Find the module name
		current = line;
		token_count = 0;

		while(*current != '\0')
		{
			if(*current == ' ')
			{
				token_count++;
				current++;

				continue;
			}

			if(token_count > 0)
			{
				break;
			}

			current++;
		}

		if(token_count == 0)
		{
			// Cannot find module name
			return false;
		}

		parse_result->module_name = current;
		parse_result->address = begin_address;
		// Demangle the name
		int status = 0;
		parse_result->function_name = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);
		parse_result->offset = begin_offset;

		return true;
	}

	String StackTrace::GetStackTraceInternal(int offset, int line_count)
	{
		void *addr_list[64];

		String log;

		int buffer_size = ::backtrace(addr_list, sizeof(addr_list) / sizeof(addr_list[0]));

		if(buffer_size == 0)
		{
			return "";
		}

		char **symbol_list = ::backtrace_symbols(addr_list, buffer_size);
		int count = (line_count >= 0) ? std::min(line_count + offset, buffer_size) : buffer_size;

		// Called by signal handler (AbortHandler -> WriteStackTrace -> GetBackTrace):
		// #0: GetBackTrace()
		// #1: WriteStackTrace()
		// #2: AbortHandler()

		// Called by GetStackTrace (GetStackTrace -> GetBackTrace);
		// #0: GetBackTrace()
		// #1: GetStackTrace()
		for(int i = offset; i < count; ++i)
		{
			char *line = symbol_list[i];

			if(line[0] == '\0')
			{
				// empty line
				continue;
			}

			ParseResult parse_result;
			bool result = false;

			result = result || ParseLinuxStyleLine(line, &parse_result);
			result = result || ParseMacOsStyleLine(line, &parse_result);

			if(result)
			{
				log.AppendFormat("#%-3d %-35s %s %s + %s\n",
				                 (i - offset),
				                 parse_result.module_name,
				                 parse_result.address,
				                 (parse_result.function_name == nullptr) ? "?" : parse_result.function_name,
				                 (parse_result.offset == nullptr) ? "0x0" : parse_result.offset
				);
			}
			else
			{
				log.AppendFormat("#%-3d || %s\n", (i - offset), line);
			}

			if(parse_result.function_name != nullptr)
			{
				free(parse_result.function_name);
			}
		}

		free(symbol_list);

		return std::move(log);
	}

	void StackTrace::WriteStackTrace(int signum, String sig_name)
	{
		char time_buffer[30];

		char file_name[32];

		String log;

		time_t t = ::time(nullptr);
		::strftime(time_buffer, sizeof(time_buffer) / sizeof(time_buffer[0]), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));

		strftime(file_name, 32, "crash_%Y%m%d.dump", localtime(&t));
		std::ofstream dump_file(file_name, std::ofstream::app);

		log.AppendFormat(
			"***** Crash dump *****\n"
			"OvenMediaEngine v%s "
			"(pid: %llu, tid: %llu)\n"
			"Signal %d (%s) %s\n",
			instance_version.CStr(),
			Platform::GetProcessId(), Platform::GetThreadId(),
			signum, sig_name.CStr(), time_buffer
		);

		dump_file << log;

		log = GetStackTraceInternal(3);

		dump_file << log;

		// need not call fstream::close() explicitly to close the file
	}
}