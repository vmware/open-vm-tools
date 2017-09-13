/*
 *  Author: bwilliams
 *  Created: June 29, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CWinServiceState_h_
#define CWinServiceState_h_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CWinServiceState {
public:
	CWinServiceState();
	virtual ~CWinServiceState();

public:
	/// Initializes the component.
	void initialize(
			const std::string& serviceName,
			const std::string& displayName,
			const std::string& description,
			const SmartPtrIWork& work);

	/// Gets the name of the service.
	std::string getServiceName() const;

	/// Gets the display name of the service.
	std::string getDisplayName() const;

	/// Gets the Description of the service.
	std::string getDescription() const;

	/// Gets interface for the implementation of the work to be
	/// performed by this service.
	SmartPtrIWork getWork() const;

public:
	/// Gets the number of Milliseconds to wait for the worker thread to stop.
	DWORD getWorkerThreadStopMs() const;

	/// Gets the number of Milliseconds that the SCM should wait for a status update.
	DWORD getScmWaitHintMs() const;

public:
	/// Gets whether the code is running as a service or as console (for debugging)
	bool getIsService() const;

	/// Puts whether the code is running as a service or as console (for debugging)
	void putIsService(
		const bool isService);

	/// Gets the handle to this service.
	SERVICE_STATUS_HANDLE getServiceHandle() const;

	/// Puts the handle to this service.
	void putServiceHandle(
		const SERVICE_STATUS_HANDLE serviceHandle);

	/// Gets the current state of this service.
	DWORD getCurrentServiceState() const;

	/// Gets the current state of this service as a String (for debugging).
	std::string getCurrentServiceStateStr() const;

	/// Puts the current state of this service that's updated by the code as it
	/// runs through various phases.
	void putCurrentServiceState(
		const DWORD currentServiceState);

public:
	/// Sends a signal to the service, telling it that it's time to stop.
	void signalServiceStop();

	/// Wait for the service to stop.
	bool waitForServiceStop(
		const uint32 timeoutMs);

	/// Sends a signal indicating that the worker thread has finished working.
	void signalWorkerThreadFinished();

	/// Wait for the worker thread to finish working.
	bool waitForWorkerThreadFinished(
		const uint32 timeoutMs);

public:
	/// Closes everything down and re-sets the component.
	void close();

	//  Wrapper function to implement API call SetServiceStatus
	void setStatus();

private:
	bool _isInitialized;

	std::string _serviceName;
	std::string _displayName;
	std::string _description;
	SmartPtrIWork _work;

	DWORD _workerThreadStopMs;
	DWORD _scmWaitHintMs;

	bool _isService;

	SERVICE_STATUS_HANDLE _serviceHandle;

	DWORD _currentServiceState;

	CThreadSignal _serviceStopSignal;
	CThreadSignal _workerThreadFinishedSignal;

	SmartPtrCAutoMutex _waitMutex;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(CWinServiceState);
};

CAF_DECLARE_SMART_POINTER(CWinServiceState);

}

#endif // #ifndef CWinServiceState_h_
