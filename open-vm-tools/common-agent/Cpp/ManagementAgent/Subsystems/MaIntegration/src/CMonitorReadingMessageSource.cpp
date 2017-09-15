/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Core/CIntMessage.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CMonitorReadingMessageSource.h"
#include "Exception/CCafException.h"

using namespace Caf;

CMonitorReadingMessageSource::CMonitorReadingMessageSource() :
		_isInitialized(false),
		_listenerStartTimeMs(0),
		_listenerRestartMs(0),
		_listenerRetryCnt(0),
		_listenerRetryMax(0),
	CAF_CM_INIT_LOG("CMonitorReadingMessageSource") {
}

CMonitorReadingMessageSource::~CMonitorReadingMessageSource() {
}

void CMonitorReadingMessageSource::initialize(
		const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");

	const SmartPtrIDocument pollerDoc = configSection->findOptionalChild("poller");
	setPollerMetadata(pollerDoc);

	_monitorDir = AppConfigUtils::getRequiredString("monitor_dir");
	_restartListenerPath = FileSystemUtils::buildPath(_monitorDir, "restartListener.txt");
	_listenerConfiguredStage2Path = FileSystemUtils::buildPath(_monitorDir, "listenerConfiguredStage2.txt");

	_scriptOutputDir = AppConfigUtils::getRequiredString(_sConfigTmpDir);
	_listenerStartupType = AppConfigUtils::getRequiredString("monitor", "listener_startup_type");
	_listenerRetryMax = AppConfigUtils::getRequiredInt32("monitor", "listener_retry_max");

	_listenerRestartMs = calcListenerRestartMs();
	CAF_CM_LOG_DEBUG_VA1("_listenerRestartMs: %d", _listenerRestartMs);

	const std::string scriptsDir = AppConfigUtils::getRequiredString("scripts_dir");
#ifdef _WIN32
	_stopListenerScript = FileSystemUtils::buildPath(scriptsDir, "stop-listener.bat");
	_startListenerScript = FileSystemUtils::buildPath(scriptsDir, "start-listener.bat");
	_isListenerRunningScript = FileSystemUtils::buildPath(scriptsDir, "is-listener-running.bat");
#else
	_stopListenerScript = FileSystemUtils::buildPath(scriptsDir, "stop-listener");
	_startListenerScript = FileSystemUtils::buildPath(scriptsDir, "start-listener");
	_isListenerRunningScript = FileSystemUtils::buildPath(scriptsDir, "is-listener-running");
#endif

	if (! FileSystemUtils::doesDirectoryExist(_monitorDir)) {
		FileSystemUtils::createDirectory(_monitorDir);
	}
	_isInitialized = true;
}

bool CMonitorReadingMessageSource::doSend(
		const SmartPtrIIntMessage&,
		int32) {
	CAF_CM_FUNCNAME("doSend");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_EXCEPTIONEX_VA1(
			UnsupportedOperationException,
			E_NOTIMPL,
			"This is not a sending channel: %s", _id.c_str());
}

SmartPtrIIntMessage CMonitorReadingMessageSource::doReceive(
		const int32 timeout) {
	CAF_CM_FUNCNAME("doReceive");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (timeout > 0) {
		CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
			"Timeout not currently supported: %s", _id.c_str());
	}

	std::string reason;
	if (FileSystemUtils::doesFileExist(_listenerConfiguredStage2Path)) {
		if (FileSystemUtils::doesFileExist(_restartListenerPath)) {
			reason = FileSystemUtils::loadTextFile(_restartListenerPath);
			FileSystemUtils::removeFile(_restartListenerPath);
			_listenerRetryCnt = 0;
			_listenerStartTimeMs = CDateTimeUtils::getTimeMs();
			restartListener(reason);
		} else {
			if (isListenerRunning()) {
				_listenerRetryCnt = 0;
				if (areSystemResourcesLow()) {
					reason = "Listener running... Stopping due to low system resources";
					stopListener(reason);
				} else if (isTimeForListenerRestart()) {
					reason = "Listener running... Restarting due to expired timeout";
					_listenerStartTimeMs = CDateTimeUtils::getTimeMs();
					restartListener(reason);
				}
			} else {
				if (_listenerStartupType.compare("Automatic") == 0) {
					if ((_listenerRetryMax < 0) || (_listenerRetryCnt < _listenerRetryMax)) {
						reason = "Listener not running... Starting - "
								+ CStringConv::toString<int32>(_listenerRetryCnt + 1) + " of "
								+ CStringConv::toString<int32>(_listenerRetryMax);
						_listenerRetryCnt++;
						_listenerStartTimeMs = CDateTimeUtils::getTimeMs();
						startListener(reason);
					} else {
						reason = "Listener not running... Retries exhausted - "
								+ CStringConv::toString<int32>(_listenerRetryCnt + 1) + " of "
								+ CStringConv::toString<int32>(_listenerRetryMax);
						CAF_CM_LOG_WARN_VA0(reason.c_str());
					}
				} else {
					reason = "Listener not running... Not Automatic startup - " + _listenerStartupType;
					CAF_CM_LOG_WARN_VA0(reason.c_str());
					_listenerRetryCnt = 0;
				}
			}
		}
	} else {
		reason = "Listener not configured";
		_listenerRetryCnt = 0;
	}

	SmartPtrCIntMessage messageImpl;
	if (! reason.empty()) {
		messageImpl.CreateInstance();
		messageImpl->initializeStr(reason,
				IIntMessage::SmartPtrCHeaders(), IIntMessage::SmartPtrCHeaders());
	}

	return messageImpl;
}

bool CMonitorReadingMessageSource::isListenerRunning() const {
	const std::string stdoutStr = executeScript(_isListenerRunningScript, _scriptOutputDir);
	return (stdoutStr.compare("true") == 0);
}

void CMonitorReadingMessageSource::startListener(
		const std::string& reason) const {
	CAF_CM_FUNCNAME_VALIDATE("startListener");

	CAF_CM_LOG_DEBUG_VA1(
			"Starting the listener - reason: %s", reason.c_str());
	executeScript(_startListenerScript, _scriptOutputDir);
}

void CMonitorReadingMessageSource::stopListener(
		const std::string& reason) const {
	CAF_CM_FUNCNAME_VALIDATE("stopListener");

	CAF_CM_LOG_DEBUG_VA1(
			"Stopping the listener - reason: %s", reason.c_str());
	executeScript(_stopListenerScript, _scriptOutputDir);
}

void CMonitorReadingMessageSource::restartListener(
		const std::string& reason) const {
	CAF_CM_FUNCNAME_VALIDATE("restartListener");

	CAF_CM_LOG_DEBUG_VA1(
			"Restarting the listener - reason: %s", reason.c_str());
	executeScript(_stopListenerScript, _scriptOutputDir);
	executeScript(_startListenerScript, _scriptOutputDir);
}

std::string CMonitorReadingMessageSource::executeScript(
	const std::string& scriptPath,
	const std::string& scriptResultsDir) const {
	CAF_CM_FUNCNAME_VALIDATE("executeScript");
	CAF_CM_VALIDATE_STRING(scriptPath);
	CAF_CM_VALIDATE_STRING(scriptResultsDir);

	Cdeqstr argv;
	argv.push_back(scriptPath);

	const std::string stdoutPath = FileSystemUtils::buildPath(
			scriptResultsDir, "stdout");
	const std::string stderrPath = FileSystemUtils::buildPath(
			scriptResultsDir, "stderr");

	ProcessUtils::runSyncToFiles(argv, stdoutPath, stderrPath);

	std::string rc;
	if (FileSystemUtils::doesFileExist(stdoutPath)) {
		rc = FileSystemUtils::loadTextFile(stdoutPath);
	}

	return rc;
}

bool CMonitorReadingMessageSource::areSystemResourcesLow() const {
	// TODO: Implement
	return false;
}

bool CMonitorReadingMessageSource::isTimeForListenerRestart() const {
	return (_listenerRestartMs > 0) && (CDateTimeUtils::calcRemainingTime(_listenerStartTimeMs, _listenerRestartMs) == 0);
}

uint64 CMonitorReadingMessageSource::calcListenerRestartMs() const {
	const uint32 listenerRestartDays = AppConfigUtils::getOptionalUint32("monitor", "listener_restart_days");
	const uint32 listenerRestartHours = AppConfigUtils::getOptionalUint32("monitor", "listener_restart_hours");
	const uint32 listenerRestartMins = AppConfigUtils::getOptionalUint32("monitor", "listener_restart_mins");
	const uint32 listenerRestartSecs = AppConfigUtils::getOptionalUint32("monitor", "listener_restart_secs");

	uint64 rc = 0;
	rc = (rc == 0) && (listenerRestartDays > 0) ? CTimeUnit::DAYS::toMilliseconds(listenerRestartDays) : rc;
	rc = (rc == 0) && (listenerRestartHours > 0) ? CTimeUnit::HOURS::toMilliseconds(listenerRestartHours) : rc;
	rc = (rc == 0) && (listenerRestartMins > 0) ? CTimeUnit::MINUTES::toMilliseconds(listenerRestartMins) : rc;
	rc = (rc == 0) && (listenerRestartSecs > 0) ? CTimeUnit::SECONDS::toMilliseconds(listenerRestartSecs) : rc;

	return rc;
}
