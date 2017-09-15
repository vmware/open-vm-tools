/*
 *	 Author: bwilliams
 *  Created: Jan 21, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CProcessUtils_H_
#define CProcessUtils_H_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE ProcessUtils {
public:
	enum COMMONAGGREGATOR_LINKAGE Priority {
		NORMAL,
		LOW,
		IDLE
	};
public:
	// Use for the "workingDirectory" parameter when you want to inherit the parents directory.
	static const std::string INHERIT_PARENT_DIRECTORY;

public:
	static void runSyncToFiles(
		const Cdeqstr& argv,
		const std::string& stdoutPath,
		const std::string& stderrPath,
		const ProcessUtils::Priority priority = NORMAL,
		const std::string workingDirectory = ProcessUtils::INHERIT_PARENT_DIRECTORY);

	static void runSync(
		const Cdeqstr& argv,
		std::string& stdoutContent,
		std::string& stderrContent,
		const ProcessUtils::Priority priority = NORMAL,
		const std::string workingDirectory = ProcessUtils::INHERIT_PARENT_DIRECTORY);

public:
	static std::string getUserName();
	static std::string getRealUserName();

private:
	static void runSync(
		const Cdeqstr& argv,
		const std::string& stdoutPath,
		const std::string& stderrPath,
		std::string& stdoutContent,
		std::string& stderrContent,
		const ProcessUtils::Priority priority,
		const std::string& workingDirectory);

	static const char** convertToCharArray(
			const Cdeqstr& argv);

	static std::string convertToString(
			const Cdeqstr& deqstr);

	static void freeMemory(
			GError *gError,
			gchar *gStdout,
			gchar *gStderr,
			const char** argvNative);

#ifdef WIN32
	static std::string readFromPipe(
		const HANDLE readPipe);
#endif

private:
	CAF_CM_DECLARE_NOCREATE(ProcessUtils);
};

}

#endif /* CProcessUtils_H_ */
