//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./signals.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config_manager.h>
#include <orchestrator/orchestrator.h>
#include <signal.h>

#include <fstream>
#include <iostream>

#include "./main_private.h"
#include "main.h"

bool g_is_terminated;

#define SIGNAL_CASE(x) \
	case x:            \
		return #x;

static const char *GetSignalName(int signum)
{
	switch (signum)
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
			return "UNKNOWN";
	}
}

typedef void (*OV_SIG_ACTION)(int signum, siginfo_t *si, void *unused);

static void AbortHandler(int signum, siginfo_t *si, void *unused)
{
#if DEBUG
	static constexpr const char *BUILD_MODE = " [debug]";
#else	// DEBUG
	static constexpr const char *BUILD_MODE = "";
#endif	// DEBUG

	char time_buffer[30]{};
	char file_name[32]{};
	time_t t = ::time(nullptr);

	logtc("OME received signal %d (%s), interrupt.", signum, GetSignalName(signum));

	std::tm local_time{};
	::localtime_r(&t, &local_time);

	::strftime(time_buffer, sizeof(time_buffer) / sizeof(time_buffer[0]), "%Y-%m-%dT%H:%M:%S%z", &local_time);
	::strftime(file_name, 32, "crash_%Y%m%d.dump", &local_time);

	std::ofstream ostream(file_name, std::ofstream::app);

	if (ostream.is_open())
	{
		auto pid = ov::Platform::GetProcessId();
		auto tid = ov::Platform::GetThreadId();

		ostream << "***** Crash dump *****" << std::endl;
		ostream << "OvenMediaEngine v" OME_VERSION OME_GIT_VERSION_EXTRA << BUILD_MODE << " received signal " << signum << " (" << GetSignalName(signum) << ")" << std::endl;
		ostream << "- Time: " << time_buffer << ", pid: " << pid << ", tid: " << tid << std::endl;
		ostream << "- Stack trace" << std::endl;

		ov::StackTrace::WriteStackTrace(ostream);

		std::ifstream istream("/proc/self/maps", std::ifstream::in);

		if (istream.is_open())
		{
			ostream << "- Module maps" << std::endl;
			ostream << istream.rdbuf();
		}
		else
		{
			ostream << "(Could not read module maps)" << std::endl;
		}

		// need not call fstream::close() explicitly to close the file
	}
	else
	{
		logte("Could not open dump file to write");
	}

	::exit(signum);
}

static void ReloadHandler(int signum, siginfo_t *si, void *unused)
{
	logti("Trying to reload configuration...");

	auto config_manager = cfg::ConfigManager::GetInstance();

	if (config_manager->ReloadConfigs() == false)
	{
		logte("An error occurred while reload configuration");
		return;
	}

	logti("Trying to apply OriginMap to Orchestrator...");

	std::vector<info::Host> host_info_list;
	// Create info::Host
	auto hosts = config_manager->GetServer()->GetVirtualHostList();
	for (const auto &host : hosts)
	{
		host_info_list.emplace_back(host);
	}

	if (ocst::Orchestrator::GetInstance()->ApplyOriginMap(host_info_list) == false)
	{
		logte("Could not reload OriginMap");
	}
}

void TerminateHandler(int signum, siginfo_t *si, void *unused)
{
	static constexpr int TERMINATE_COUNT = 3;
	static int signal_count = 0;

	signal_count++;

	if (signal_count == TERMINATE_COUNT)
	{
		logtc("The termination request has been made %d times. OME is forcibly terminated.", TERMINATE_COUNT, signum);
		exit(1);
	}
	else
	{
		logtc("Caught terminate signal %d. Trying to terminating... (Repeat %d more times to forcibly terminate)", signum, (TERMINATE_COUNT - signal_count));
	}

	g_is_terminated = true;
}

struct sigaction GetSigAction(OV_SIG_ACTION action)
{
	struct sigaction sa
	{
	};

	sa.sa_flags = SA_SIGINFO;

	// sigemptyset is a macro on macOS, so :: breaks compilation
#if defined(__APPLE__)
	sigemptyset(&sa.sa_mask);
#else
	::sigemptyset(&sa.sa_mask);
#endif

	sa.sa_sigaction = action;

	return std::move(sa);
}

// Configure abort signal
//
// Intentional signals (ignore)
//     SIGQUIT, SIGINT, SIGTERM, SIGTRAP, SIGHUP, SIGKILL
//     SIGVTALRM, SIGPROF, SIGALRM
bool InitializeAbortSignal()
{
	bool result = true;
	auto sa = GetSigAction(AbortHandler);

	// Core dumped signal
	result = result && (::sigaction(SIGABRT, &sa, nullptr) == 0);  // assert()
	result = result && (::sigaction(SIGSEGV, &sa, nullptr) == 0);  // illegal memory access
	result = result && (::sigaction(SIGBUS, &sa, nullptr) == 0);   // illegal memory access
	result = result && (::sigaction(SIGILL, &sa, nullptr) == 0);   // execute a malformed instruction.
	result = result && (::sigaction(SIGFPE, &sa, nullptr) == 0);   // divide by zero
	result = result && (::sigaction(SIGSYS, &sa, nullptr) == 0);   // bad system call
	result = result && (::sigaction(SIGXCPU, &sa, nullptr) == 0);  // cpu time limit exceeded
	result = result && (::sigaction(SIGXFSZ, &sa, nullptr) == 0);  // file size limit exceeded

	// Terminated signal
	result = result && (::sigaction(SIGPIPE, &sa, nullptr) == 0);  // write on a pipe with no one to read it
#if IS_LINUX
	result = result && (::sigaction(SIGPOLL, &sa, nullptr) == 0);  // pollable event
#endif															   // IS_LINUX

	return result;
}

// Configure reload signal
bool InitializeReloadSignal()
{
	auto sa = GetSigAction(ReloadHandler);
	bool result = true;

	result = result && (::sigaction(SIGHUP, &sa, nullptr) == 0);

	return result;
}

// Configure terminate signal
bool InitializeTerminateSignal()
{
	auto sa = GetSigAction(TerminateHandler);
	bool result = true;

	result = result && (::sigaction(SIGINT, &sa, nullptr) == 0);

	g_is_terminated = false;

	return result;
}

bool InitializeSignals()
{
	//	 1) SIGHUP		 2) SIGINT		 3) SIGQUIT		 4) SIGILL		 5) SIGTRAP
	//	 6) SIGABRT		 7) SIGBUS		 8) SIGFPE		 9) SIGKILL		10) SIGUSR1
	//	11) SIGSEGV		12) SIGUSR2		13) SIGPIPE		14) SIGALRM		15) SIGTERM
	//	16) SIGSTKFLT	17) SIGCHLD		18) SIGCONT		19) SIGSTOP		20) SIGTSTP
	//	21) SIGTTIN		22) SIGTTOU		23) SIGURG		24) SIGXCPU		25) SIGXFSZ
	//	26) SIGVTALRM	27) SIGPROF		28) SIGWINCH	29) SIGIO		30) SIGPWR
	//	31) SIGSYS		34) SIGRTMIN	35) SIGRTMIN+1	36) SIGRTMIN+2	37) SIGRTMIN+3
	//	38) SIGRTMIN+4	39) SIGRTMIN+5	40) SIGRTMIN+6	41) SIGRTMIN+7	42) SIGRTMIN+8
	//	43) SIGRTMIN+9	44) SIGRTMIN+10	45) SIGRTMIN+11	46) SIGRTMIN+12	47) SIGRTMIN+13
	//	48) SIGRTMIN+14	49) SIGRTMIN+15	50) SIGRTMAX-14	51) SIGRTMAX-13	52) SIGRTMAX-12
	//	53) SIGRTMAX-11	54) SIGRTMAX-10	55) SIGRTMAX-9	56) SIGRTMAX-8	57) SIGRTMAX-7
	//	58) SIGRTMAX-6	59) SIGRTMAX-5	60) SIGRTMAX-4	61) SIGRTMAX-3	62) SIGRTMAX-2
	//	63) SIGRTMAX-1	64) SIGRTMAX

	return InitializeAbortSignal() &&
		   InitializeReloadSignal() &&
		   InitializeTerminateSignal();
}