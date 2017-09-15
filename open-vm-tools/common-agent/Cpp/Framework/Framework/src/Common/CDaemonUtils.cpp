/*
 *	 Author: mdonahue
 *  Created: Jun 9, 2011
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CLoggingUtils.h"
#include "CDaemonUtils.h"

#include <syslog.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <sys/resource.h>
#include <dlfcn.h>

#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif

using namespace Caf;

extern "C" void DaemonUtilsCrashHandler(int32 sigNum, siginfo_t *info, void *context);

///////////////////////////////////////////////////////////////////////////////
//
// MakeDaemon
// This function makes the process a daemon process
//
// NOTE: The following function has numerous coding standard violations that
//	 cannot be/should not be corrected by CAF common classes and macros.
//
///////////////////////////////////////////////////////////////////////////////
void CDaemonUtils::MakeDaemon(
		int32 argc,
		char** argv,
		const std::string& procPath,
		const std::string& procName,
		void(*pfnShutdownHandler)(int32 signalNum),
		bool& isDaemonized,
		bool& logInfos) {

	const std::string logProcName = procName.empty() ? "CDaemonUtils" : procName;
	::openlog(logProcName.c_str(), LOG_PID, LOG_USER);
	::atexit(::closelog);
	::syslog(LOG_INFO, "Initializing %s", logProcName.c_str());

	isDaemonized = true;
	logInfos = false;
	bool enableCrashHandlers = true;

	std::string userName;
	std::string groupName;
	std::string rootDir;
	int32 irc = 0;
	while ((irc = ::getopt(argc, argv, "u:g:r:vnc")) != -1) {
		switch (irc) {
		case 'u':
			userName = optarg;
			break;
		case 'g':
			groupName = optarg;
			break;
		case 'r':
			rootDir = optarg;
			break;
		case 'n':
			isDaemonized = false;
			break;
		case 'c':
			enableCrashHandlers = false;
			break;
		case 'v':
			logInfos = true;
			break;
		default:
			::syslog(LOG_WARNING, "Unknown option '%c', ignoring", irc);
		}
	}

	if (logInfos) {
		::syslog(
				LOG_INFO,
				"Got user %s",
				userName.empty() ? "<not provided>" : userName.c_str());
		::syslog(
				LOG_INFO,
				"Got group %s",
				groupName.empty() ? "<not provided>" : groupName.c_str());
	}

	if (isDaemonized) {
		if (logInfos) {
			::syslog(LOG_INFO, "Daemonizing");
		}

		// This bizarre check is necessary because the console appender writes to stdout
		// when a daemon, which messes up the listener some of the file reads - plus, it
		// just doesn't make sense for a daemon to write to the console.
		if (CLoggingUtils::isConsoleAppenderUsed()) {
			::syslog(LOG_ERR, "Daemon cannot use console appender");
			::exit(2);
		}

		// first fork ourselves off
		if (logInfos) {
			::syslog(LOG_INFO, "Daemon forking");
		}
		::closelog();

		int32 childPid = ::fork();
		if (childPid < 0) {
			::syslog(LOG_ERR, "Cannot fork child - %s", ::strerror(errno));
			::exit(2);
		} else if (childPid > 0) {
			::exit( 0 ); // we're done because we are the parent
		}

		// so we are the child - setsid causes this process to be a new
		// session leader, process group leader and has no controlling terminal
		if (static_cast<pid_t>(-1) == ::setsid())	{
			::syslog(
					LOG_ERR,
					"Unable to become a session leader - %s",
					::strerror(errno));
			::exit(2);
		}

		// Ignore the signal from this parent terminating
		::signal(SIGHUP, SIG_IGN);

		// and fork again	- puts the init process in charge of cleaning us up
		childPid = ::fork();
		if (childPid < 0)	{
			syslog( LOG_ERR, "Cannot fork 2nd child - %s", ::strerror(errno));
			::exit(2);
		} else if (childPid > 0) {
			::exit( 0 ); // we're done because we are the first child
		}

		// close any open file descriptors
		int32 maxFd = ::sysconf(_SC_OPEN_MAX);
		if (maxFd < 0) {
			maxFd = OPEN_MAX;
		}

		for (int32 fd = 0; fd < maxFd; ++fd) {
			::close(fd);
		}

		// and re-open syslog
		::openlog(logProcName.c_str(), LOG_CONS | LOG_PID, LOG_USER);
		errno = 0;
	}

	// set a signal handler to catch SIGTERM & SIGINT
	struct sigaction sigActionInfo;
	if (pfnShutdownHandler) {
		::memset(&sigActionInfo, 0, sizeof(struct sigaction));
		sigActionInfo.sa_flags = 0;
		sigActionInfo.sa_handler = pfnShutdownHandler;
		sigemptyset(&sigActionInfo.sa_mask);

		if (-1 == ::sigaction(SIGTERM, &sigActionInfo, NULL)) {
			::syslog(
					LOG_ERR,
					"Unable to setup shutdown signal handler - %s",
					::strerror(errno));
			::exit(2);
		}

		if (-1 == ::sigaction(SIGINT, &sigActionInfo, NULL))
		{
			::syslog(
					LOG_ERR,
					"Unable to setup interrupt signal handler - %s",
					::strerror(errno));
			::exit(2);
		}
	} else {
		::syslog(LOG_WARNING, "No shutdown handler function was supplied.");
	}

	// ignore any terminal signals
	#ifdef SIGTTOU
		::signal(SIGTTOU, SIG_IGN);
	#endif
	#ifdef SIGTTIN
		::signal(SIGTTIN, SIG_IGN);
	#endif
	#ifdef SIGTSTP
		::signal(SIGTSTP, SIG_IGN);
	#endif

	// set us up as the specified group
	if (!groupName.empty()) {
		struct group* groupInfo = ::getgrnam(groupName.c_str());
		if (groupInfo) {
			if (logInfos) {
				::syslog(LOG_INFO, "Switching to group %d", groupInfo->gr_gid);
			}
			if (0 != ::setgid(groupInfo->gr_gid)) {
				::syslog(
						LOG_ERR,
						"Unable to become group %s - %s",
						groupName.c_str(),
						::strerror(errno));
				::exit(2);
			}
		} else {
			::syslog(
					LOG_ERR,
					"Unable to find group info for %s - %s",
					groupName.c_str(),
					::strerror(errno));
			::exit(2);
		}
	}

	// set us up as the specified user
	if (!userName.empty())	{
		struct passwd * passwdInfo = ::getpwnam(userName.c_str());
		if (passwdInfo) {
			if (logInfos) {
				::syslog(LOG_INFO, "Switching to user %d", passwdInfo->pw_uid);
			}
			if (0 != ::setuid(passwdInfo->pw_uid)) {
				::syslog(
						LOG_ERR,
						"Unable to become user %s - %s",
						userName.c_str(),
						::strerror(errno));
				::exit(2);
			}
		} else {
			::syslog(
					LOG_ERR,
					"Unable to find login info for %s - %s",
					userName.c_str(),
					::strerror(errno));
			::exit(2);
		}
	}

	// Move the current directory to the specified directory
	// to make sure we don't hold a file system open.
	if (rootDir.empty())	{
		if (logInfos) {
			::syslog(LOG_INFO, "Switching to directory of %s", procPath.c_str());
		}
		const char * lastSlash = ::strrchr(procPath.c_str(), '/');
		if (*lastSlash) {
			std::string directory = procPath;
			directory.erase(directory.rfind('/'));
			if (logInfos) {
				::syslog(LOG_INFO, "chdir %s", directory.c_str());
			}
			::chdir(directory.c_str());
		} else {
			if (logInfos) {
				::syslog(LOG_INFO, "chdir /");
			}
			::chdir("/");
		}
	} else {
		if (logInfos) {
			::syslog(LOG_INFO, "Switching to directory %s", rootDir.c_str());
		}
		if (-1 == ::chdir(rootDir.c_str()))
		{
			::syslog(
					LOG_ERR,
					"::chdir to %s failed - %s",
					rootDir.c_str(),
					::strerror(errno));
		}
	}

	// set our umask correctly - disable all world access
	if (logInfos) {
		::syslog(LOG_INFO, "umask 0007");
	}
	::umask(0007);

	// boost our open file limit
	struct rlimit rlimitInfo;
	::getrlimit(RLIMIT_NOFILE, &rlimitInfo);
	if (rlimitInfo.rlim_cur < rlimitInfo.rlim_max) {
		if (logInfos) {
#ifdef __x86_64__
			::syslog(
					LOG_INFO,
					"rlimit change #files from %lld to %lld",
					(long long) rlimitInfo.rlim_cur,
					(long long) rlimitInfo.rlim_max);
#else
			::syslog(
					LOG_INFO,
					"rlimit change #files from %d to %d",
					rlimitInfo.rlim_cur,
					rlimitInfo.rlim_max);
#endif
		}
		rlimitInfo.rlim_cur = rlimitInfo.rlim_max;
		::setrlimit(RLIMIT_NOFILE, &rlimitInfo);
	} else {
		if (logInfos) {
#ifdef __x86_64__
			::syslog(
					LOG_INFO,
					"rlimit #files already at maximum of %lld",
					(long long) rlimitInfo.rlim_cur);
#else
			::syslog(
					LOG_INFO,
					"rlimit #files already at maximum of %d",
					rlimitInfo.rlim_cur);
#endif
		}
	}

	if (enableCrashHandlers)
	{
		// Set up the crash handler
	    struct sigaction newAction;
	    struct sigaction oldAction;
	    ::memset(&newAction, 0, sizeof(struct sigaction));
	    ::memset(&oldAction, 0, sizeof(struct sigaction));

	    newAction.sa_sigaction = DaemonUtilsCrashHandler;
	    newAction.sa_flags = SA_SIGINFO | SA_RESETHAND;
	    if (sigfillset(&newAction.sa_mask) == -1) {
			::syslog(
					LOG_ERR,
					"Unable to fill crash handler signal set - %s",
					::strerror(errno));
	    }
	    else if (::sigaction(SIGILL, &newAction, &oldAction) == -1  ||
					::sigaction(SIGSEGV, &newAction, &oldAction) == -1  ||
					::sigaction(SIGFPE, &newAction, &oldAction) == -1  ||
					::sigaction(SIGBUS, &newAction, &oldAction) == -1)
	    {
			::syslog(
					LOG_ERR,
					"Unable to set crash handler signal handler %s",
					::strerror(errno));
	    }
	}

	if (logInfos)
		::syslog(LOG_INFO, "Initialized");
}

extern "C" void DaemonUtilsCrashHandler(int32 sigNum, siginfo_t *info, void *context)
{
	CAF_CM_STATIC_FUNC_LOG("CDaemonUtils", "DaemonUtilsCrashHandler");

	try {
		std::string message("Got Signal ");
		switch (sigNum) {
		case SIGSEGV:
			message += "[SEGV";
			break;
		case SIGBUS:
			message += "[BUS";
			break;
		case SIGFPE:
			message += "[FPE";
			break;

		case SIGILL:
			message += "[ILL";
			break;

		default:
			message +=
					"[UNKNOWN SIGNAL (" +
					CStringConv::toString(sigNum) +
					")";
			break;
		}
		message += " code=" + CStringConv::toString(info->si_code) + "]";
		message += " Fault Addr[" + CStringConv::toString(info->si_addr) + "] ";

#if defined ( __linux__ ) || defined ( __APPLE__ )
		Dl_info dlInfo;
		if (0 != ::dladdr(info->si_addr, &dlInfo)) {
			if (dlInfo.dli_fname != NULL) {
				message += " Module [";
				message += dlInfo.dli_fname;
			}
			if (dlInfo.dli_sname != NULL) {
				message += " Symbol [";
				message += dlInfo.dli_sname;
			}
		}
#else
	#error "Need to determine if DlInfo is available"
#endif

		::syslog(LOG_ERR, "%s", message.c_str());
		CAF_CM_LOG_ERROR_VA0(message.c_str());
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
	::exit(-1);
}

