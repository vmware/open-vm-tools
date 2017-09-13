/*
 *	 Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CWinService_h_
#define CWinService_h_

namespace Caf {

class CWinService {
public:
	/// Initializes the static service class.
	static void initialize(
		const SmartPtrIWork& work);

	/// Parses the command line and executes the server
	static void execute(int32 argc, char** argv);

private:
	/// Spawns the worker thread to run the service
	static void run();

	/// Runs the service in console mode for debugging
	static void runAsConsole();

private:
	/// Each Service (there can be more than one) exposes its own main function
	/// This function's pointer was passed into the SCM in the run method
	/// For each of these main functions the SCM will create a thread.
	static void CALLBACK serviceMain(
		const DWORD cdwArgc,
		LPCWSTR* plpwszArgv);

	/// Notification callback function for the SCM that's called when the state of
	/// the Service must be changed.
	static DWORD CALLBACK scmHandlerEx(
		DWORD dwCommand,
		DWORD dwEventType,
		LPVOID lpEventData,
		LPVOID lpContext );

private:
	// Creates the service's main working thread which performs the work of this service.
	static void createWorkerThread();

private:
	/// When CWinService::createWorkerThread() creates the helper-thread,
	///	it calls this global function. This function simply redirects the thread
	///	back into the main class.
	friend uint32 __stdcall serviceWorkerThreadFunc(
		void* pThreadContext);

public:
	static bool s_isInitialized;
	static SmartPtrCWinServiceState s_winServiceState;
	static SmartPtrCWinServiceInstance s_winServiceInstance;

private:
	CAF_CM_DECLARE_NOCREATE(CWinService);
};

}

#endif // #ifndef CWinService_h_
