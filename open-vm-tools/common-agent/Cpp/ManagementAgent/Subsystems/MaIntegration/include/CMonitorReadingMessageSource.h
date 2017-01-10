/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _MaIntegration_CMonitorReadingMessageSource_h_
#define _MaIntegration_CMonitorReadingMessageSource_h_

#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Integration/Core/CAbstractPollableChannel.h"

namespace Caf {

class CMonitorReadingMessageSource :
	public CAbstractPollableChannel {
private:
	typedef std::map<std::string, bool> CFileCollection;
	CAF_DECLARE_SMART_POINTER(CFileCollection);

public:
	CMonitorReadingMessageSource();
	virtual ~CMonitorReadingMessageSource();

public:
	void initialize(
		const SmartPtrIDocument& configSection);

protected: // CAbstractPollableChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

	SmartPtrIIntMessage doReceive(const int32 timeout);

private:
	bool isListenerRunning() const;

	void startListener(
			const std::string& reason) const;

	void stopListener(
			const std::string& reason) const;

	void restartListener(
			const std::string& reason) const;

	std::string executeScript(
			const std::string& scriptPath,
			const std::string& scriptResultsDir) const;

	bool areSystemResourcesLow() const;

	bool isTimeForListenerRestart() const;

	uint64 calcListenerRestartMs() const;

private:
	bool _isInitialized;
	std::string _id;

	uint64 _listenerStartTimeMs;
	uint64 _listenerRestartMs;

	std::string _monitorDir;
	std::string _restartListenerPath;
	std::string _listenerConfiguredStage2Path;

	std::string _scriptOutputDir;
	std::string _stopListenerScript;
	std::string _startListenerScript;
	std::string _isListenerRunningScript;

	std::string _listenerStartupType;
	int32 _listenerRetryCnt;
	int32 _listenerRetryMax;

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CMonitorReadingMessageSource);
};

CAF_DECLARE_SMART_POINTER(CMonitorReadingMessageSource);

}

#endif // #ifndef _MaIntegration_CMonitorReadingMessageSource_h_
