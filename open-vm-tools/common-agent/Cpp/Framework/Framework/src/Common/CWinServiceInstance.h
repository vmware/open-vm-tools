/*
 *	 Author: bwilliams
 *  Created: June 25, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CWinServiceInstance_h_
#define CWinServiceInstance_h_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CWinServiceInstance {
public:
	typedef enum EServerMode {
		EModeUnknown,
		EModeRunAsService,
		EModeRunAsConsole,
		EModeRegister,
		EModeUnregister
	};

	struct SCommandLineParams {
		EServerMode _eMode;
	};

public:
	CWinServiceInstance();
	virtual ~CWinServiceInstance();

public:
	/// Initializes the component.
	void initialize(
		const SmartPtrCWinServiceState& winServiceState);

	/// Monitors for the signal indicating that the service should be stopped.
	void runService();

	/// Runs in the worker thread and performs the work of this service.
	void runWorkerThread();

public:
	/// Processes the command-line arguments.
	SCommandLineParams processCommandLine(
		int32 argc,
		char** argv) const;

	/// Installs the service.
	void install(
			const std::string& fileName) const;

	/// Uninstalls the service.
	void uninstall() const;

private:
	/// Produces the usage message.
	void usage(
			const std::string& serviceName) const;

public:
	bool _isInitialized;
	SmartPtrCWinServiceState _winServiceState;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CWinServiceInstance);
};

CAF_DECLARE_SMART_POINTER(CWinServiceInstance);

}

#endif // #ifndef CWinServiceInstance_h_
