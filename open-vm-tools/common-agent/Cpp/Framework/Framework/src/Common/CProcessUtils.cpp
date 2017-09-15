/*
 *	Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Exception/CCafException.h"
#include "CProcessUtils.h"
#ifndef WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif


using namespace Caf;

const std::string ProcessUtils::INHERIT_PARENT_DIRECTORY;

void ProcessUtils::runSyncToFiles(
	const Cdeqstr& argv,
	const std::string& stdoutPath,
	const std::string& stderrPath,
	const ProcessUtils::Priority priority,
	const std::string workingDirectory) {

	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ProcessUtils", "runSyncToFiles");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(argv);
		// stdoutPath is optional
		// stderrPath is optional

		std::string stdoutContent;
		std::string stderrContent;
		ProcessUtils::runSync(
			argv, stdoutPath, stderrPath, stdoutContent, stderrContent, priority, workingDirectory);
	}
	CAF_CM_EXIT;
}

void ProcessUtils::runSync(
	const Cdeqstr& argv,
	std::string& stdoutContent,
	std::string& stderrContent,
	const ProcessUtils::Priority priority,
	const std::string workingDirectory) {

	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ProcessUtils", "runSync");

	CAF_CM_ENTER
	{
		CAF_CM_VALIDATE_STL(argv);

		ProcessUtils::runSync(
				argv, std::string(), std::string(), stdoutContent, stderrContent, priority, workingDirectory);
	}
	CAF_CM_EXIT;
}

#ifdef WIN32
void ProcessUtils::runSync(
	const Cdeqstr& argv,
	const std::string& stdoutPath,
	const std::string& stderrPath,
	std::string& stdoutContent,
	std::string& stderrContent,
	const ProcessUtils::Priority priority,
	const std::string& workingDirectory) {
	CAF_CM_STATIC_FUNC_LOG( "CProcessUtils", "runSync(Win)" );

	const uint32 maxCmdLineLen = 1024;

	PROCESS_INFORMATION processInfo;
	HANDLE stdoutReadPipe = NULL;
	HANDLE stdoutWritePipe = NULL;
	HANDLE stderrReadPipe = NULL;
	HANDLE stderrWritePipe = NULL;

	try {
		CAF_CM_VALIDATE_STL(argv);

		const std::string cmdLine = convertToString(argv);

		if (cmdLine.length() > maxCmdLineLen) {
			CAF_CM_EXCEPTION_VA1(0, "Command-line too long: \"%s\"",
				cmdLine.c_str());
		}

		CAF_CM_LOG_INFO_VA1("Running command - %s", cmdLine.c_str());

		char cmdLineBuf[maxCmdLineLen + 1];
		::strcpy_s(cmdLineBuf, cmdLine.c_str());

		SECURITY_ATTRIBUTES securityAttributes;
		::ZeroMemory(&securityAttributes, sizeof(securityAttributes));
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		//Create pipes to write and read stdout/stderr
		BOOL apiRc = ::CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &securityAttributes, 0);
		if (apiRc == FALSE) {
			const DWORD errorCode = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(errorCode);
			CAF_CM_EXCEPTION_VA2(errorCode, "Failed to create the stdout pipe - cmdLine: \"%s\", msg: \"%s\"",
				cmdLine.c_str(), errorMsg.c_str());
		}

		apiRc = ::CreatePipe(&stderrReadPipe, &stderrWritePipe, &securityAttributes, 0);
		if (apiRc == FALSE) {
			const DWORD errorCode = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(errorCode);
			CAF_CM_EXCEPTION_VA2(errorCode, "Failed to create the stderr pipe - cmdLine: \"%s\", msg: \"%s\"",
				cmdLine.c_str(), errorMsg.c_str());
		}

		STARTUPINFOA startupInfo;
		::ZeroMemory(&startupInfo, sizeof(startupInfo));
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;
		startupInfo.hStdInput = NULL;
		startupInfo.hStdOutput = stdoutWritePipe;
		startupInfo.hStdError = stderrWritePipe;

		::ZeroMemory(&processInfo, sizeof(processInfo));

		DWORD dwCreationFlags = CREATE_NO_WINDOW;
		switch (priority) {
			case LOW:
				dwCreationFlags |= BELOW_NORMAL_PRIORITY_CLASS;
				break;

			case IDLE:
				dwCreationFlags |= IDLE_PRIORITY_CLASS;
				break;

			case NORMAL:
			default:
				dwCreationFlags |= NORMAL_PRIORITY_CLASS;
				break;
		}

		// NORMAL_PRIORITY_CLASS BELOW_NORMAL_PRIORITY_CLASS IDLE_PRIORITY_CLASS
		// Create the process
		apiRc = ::CreateProcessA(
			NULL,				// Image Name
			cmdLineBuf,		// Command Line
			NULL,				// Security Attributes for the Process
			NULL,				// Security Attributes for the Thread
			TRUE,				// Inherit handles?
			dwCreationFlags,	// Creation flags
			NULL,				// Environment
			workingDirectory.length() == 0 ? NULL : workingDirectory.c_str(),
			&startupInfo,	// Startup Info
			&processInfo);	// Process Info

		if (apiRc == FALSE) {
			const DWORD errorCode = ::GetLastError();
			const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(errorCode);
			CAF_CM_EXCEPTION_VA2(errorCode, "Failed to invoke \"%s\", msg: \"%s\"",
				cmdLine.c_str(), errorMsg.c_str());
		} else {
			// Successfully created the process.  Wait for it to finish.
			::WaitForSingleObject(processInfo.hProcess, INFINITE);

			// Get the exit code.
			DWORD exitCode = 0;
			apiRc = ::GetExitCodeProcess(processInfo.hProcess, &exitCode);
			if (apiRc == FALSE) {
				const DWORD errorCode = ::GetLastError();
				const std::string errorMsg = BasePlatform::PlatformApi::GetApiErrorMessage(errorCode);
				CAF_CM_EXCEPTION_VA2(errorCode, "Executed command but couldn't get exit code - cmdLine: \"%s\", msg: \"%s\"",
					cmdLine.c_str(), errorMsg.c_str());
			}

			::CloseHandle(stdoutWritePipe);
			::CloseHandle(stderrWritePipe);
			stdoutWritePipe = NULL;
			stderrWritePipe = NULL;

			stdoutContent = readFromPipe(stdoutReadPipe);
			stderrContent = readFromPipe(stderrReadPipe);

			if (!stdoutContent.empty() && !stdoutPath.empty()) {
				FileSystemUtils::saveTextFile(stdoutPath, stdoutContent);
			}

			if (!stderrContent.empty() && !stderrPath.empty()) {
				FileSystemUtils::saveTextFile(stderrPath, stderrContent);
			}

			std::ostringstream msgStream;
			if (exitCode != 0) {
				msgStream << "Command failed - exitCode: " << exitCode;
				msgStream << ", cmdLine: \"" << cmdLine << "\"";
				msgStream << ", stdout: \"" << stdoutContent.c_str() << "\"";
				msgStream << ", stderr: \"" << stderrContent.c_str() << "\"";
				std::string msg = msgStream.str();

				CAF_CM_LOG_WARN_VA0(msg.c_str());
				CAF_CM_EXCEPTIONEX_VA0(ProcessFailedException, exitCode, msg);
			}

			msgStream << "Command succeeded - cmdLine: \"" << cmdLine << "\"; output: ";
			msgStream << ", stdout: \"" << stdoutContent.c_str() << "\"";
			msgStream << ", stderr: \"" << stderrContent.c_str() << "\"";
			CAF_CM_LOG_DEBUG_VA0(msgStream.str().c_str());
		}
	}
	CAF_CM_CATCH_CAF
	CAF_CM_CATCH_DEFAULT

	try {
		if (processInfo.hProcess != NULL) {
			::CloseHandle(processInfo.hProcess);
		}
		if (processInfo.hThread != NULL) {
			::CloseHandle(processInfo.hThread);
		}
		if (stdoutReadPipe != NULL) {
			::CloseHandle(stdoutReadPipe);
		}
		if (stdoutWritePipe != NULL) {
			::CloseHandle(stdoutWritePipe);
		}
		if (stderrReadPipe != NULL) {
			::CloseHandle(stderrReadPipe);
		}
		if (stderrWritePipe != NULL) {
			::CloseHandle(stderrWritePipe);
		}
	}
	CAF_CM_CATCH_DEFAULT
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

std::string ProcessUtils::readFromPipe(
	const HANDLE readPipe) {
	CAF_CM_STATIC_FUNC_LOG( "CProcessUtils", "readFromPipe" );

	const uint32 maxOutBufSize = 1024;

	std::string rc = "";

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_PTR(readPipe);

		char readBuf[maxOutBufSize+1];
		DWORD numberOfBytesRead = 0;
		for (BOOL apiRc = ::ReadFile(readPipe, readBuf, maxOutBufSize, &numberOfBytesRead, NULL);
			apiRc != FALSE;
			apiRc = ::ReadFile(readPipe, readBuf, maxOutBufSize, &numberOfBytesRead, NULL)) {
			readBuf[numberOfBytesRead] = '\0';
			rc += std::string(readBuf);
		}
	}
	CAF_CM_EXIT;

	return rc;
}
#else

void SpawnChildSetup(gpointer data) {
	int niceLevel = *((int*) data);
	setpriority(PRIO_PROCESS, 0, niceLevel);
}

void ProcessUtils::runSync(
	const Cdeqstr& argv,
	const std::string& stdoutPath,
	const std::string& stderrPath,
	std::string& stdoutContent,
	std::string& stderrContent,
	const ProcessUtils::Priority priority,
	const std::string& workingDirectory) {
	CAF_CM_STATIC_FUNC_LOG( "CProcessUtils", "runSync(NotWin)" );

	GError *gError = NULL;
	gchar *gStdout = NULL;
	gchar *gStderr = NULL;
	const char** argvNative = NULL;

	try {
		CAF_CM_VALIDATE_STL(argv);

		const std::string cmdLine = convertToString(argv);

		// Each string in argvNative points to the internal memory in
		// the corresponding string in argv. So int32 as argvNative and
		// argv have (at least) the same scope, everything should be fine.
		argvNative = convertToCharArray(argv);

		int niceLevel;
		switch (priority) {
			case LOW:
				niceLevel = 10;
				break;

			case IDLE:
				niceLevel = 19;
				break;

			case NORMAL:
			default:
				niceLevel = 0;
				break;
		}

		CAF_CM_LOG_INFO_VA1("Running command - %s", cmdLine.c_str());

		gint gStatus = 0;
		const bool isSuccessful = g_spawn_sync(
			workingDirectory.length() == 0 ? NULL : workingDirectory.c_str(),
			const_cast<char**>(argvNative),
			NULL,							// child's environment, or NULL to inherit parent's
			static_cast<GSpawnFlags>(0),	// GSpawnFlags - the defaults are fine
			&SpawnChildSetup,				// child_setup - function to run in the child just before exec()
			&niceLevel,						// user_data - user data for child_setup
			&gStdout,
			&gStderr,
			&gStatus,
			&gError);

		stdoutContent = (gStdout == NULL) ? std::string() : gStdout;
		stderrContent = (gStderr == NULL) ? std::string() : gStderr;

		if (!stdoutContent.empty() && !stdoutPath.empty()) {
			FileSystemUtils::saveTextFile(stdoutPath, stdoutContent);
		}
		if (!stderrContent.empty() && !stderrPath.empty()) {
			FileSystemUtils::saveTextFile(stderrPath, stderrContent);
		}

		std::ostringstream msgStream;
		if(!isSuccessful || (WIFEXITED(gStatus) == FALSE) || (WEXITSTATUS(gStatus) != 0)) {
			const std::string errorMessage = (gError == NULL) ? std::string() : gError->message;
			const int32 errorCode = (gError == NULL) ? 0 : gError->code;

			msgStream << "Failed to invoke command - errorCode: " << errorCode;
			msgStream << ", errorMessage: \"" << errorMessage.c_str() << "\"";
			msgStream << ", cmdLine: \"" << cmdLine << "\"";
			msgStream << ", stdout: \"" << stdoutContent.c_str() << "\"";
			msgStream << ", stderr: \"" << stderrContent.c_str() << "\"";
			std::string msg = msgStream.str();

			CAF_CM_LOG_WARN_VA0(msg.c_str());
			CAF_CM_EXCEPTIONEX_VA0(ProcessFailedException, errorCode, msg);
		}

		msgStream << "Command succeeded - cmdLine: \"" << cmdLine << "\"; output: ";
		msgStream << ", stdout: \"" << stdoutContent.c_str() << "\"";
		msgStream << ", stderr: \"" << stderrContent.c_str() << "\"";
		CAF_CM_LOG_DEBUG_VA0(msgStream.str().c_str());

		freeMemory(gError, gStdout, gStderr, argvNative);
	}
	catch(...) {
		freeMemory(gError, gStdout, gStderr, argvNative);
		throw;
	}
}
#endif

std::string ProcessUtils::getUserName() {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ProcessUtils", "getUserName");

	std::string rc;

	CAF_CM_ENTER {
		const gchar* gUserName = g_get_user_name();
		CAF_CM_VALIDATE_PTR(gUserName);

		rc = gUserName;
	}
	CAF_CM_EXIT;

	return rc;
}

std::string ProcessUtils::getRealUserName() {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("ProcessUtils", "getRealUserName");

	std::string rc;

	CAF_CM_ENTER {
		const gchar* gUserName = g_get_real_name();
		CAF_CM_VALIDATE_PTR(gUserName);

		rc = gUserName;
	}
	CAF_CM_EXIT;

	return rc;
}

const char** ProcessUtils::convertToCharArray(
		const Cdeqstr& deqstr) {
	int32 rcIndex = 0;
	const char** rc = new const char*[deqstr.size() + 1];
	for (TConstIterator<Cdeqstr> strIter(deqstr); strIter; strIter++) {
		rc[rcIndex++] = (*strIter).c_str();
	}

	rc[rcIndex++] = NULL;

	return rc;
}

std::string ProcessUtils::convertToString(const Cdeqstr& deqstr) {
	std::string rc;
	for (TConstIterator<Cdeqstr> strIter(deqstr); strIter; strIter++) {
		std::string str = *strIter;
#ifdef WIN32
		if (rc.empty() && str.find(" ") != std::string::npos) {
			str = "\"" + *strIter + "\"";
		}
#endif
		rc += str + std::string(" ");
	}

	return rc;
}

void ProcessUtils::freeMemory(
		GError *gError,
		gchar *gStdout,
		gchar *gStderr,
		const char** argvNative) {
	if(gError != NULL) {
		g_error_free(gError);
		gError = NULL;
	}
	g_free(gStdout);
	g_free(gStderr);
	if (argvNative != NULL) {
		delete[] argvNative;
	}
}
