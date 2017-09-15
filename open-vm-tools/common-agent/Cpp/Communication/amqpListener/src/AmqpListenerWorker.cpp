/*
 *  Created on: Aug 20, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */
// NOTE:  windows.h defines macros for min and max.  Specifying NOMINMAX to prevent
// macro definition.  Must be specified before windows.h is included.
#define NOMINMAX

#include "stdafx.h"

#include "Integration/Core/CIntegrationAppContext.h"
#include "AmqpListenerWorker.h"
#include "Common/CLoggingUtils.h"
#include "amqpClient/api/AMQExceptions.h"

using namespace Caf;

AmqpListenerWorker::AmqpListenerWorker() :
	CAF_CM_INIT_LOG("AmqpListenerWorker") {
	CAF_THREADSIGNAL_INIT;
	_stopSignal.initialize("AmqpListenerWorker::stopSignal");
}

AmqpListenerWorker::~AmqpListenerWorker() {
}

void AmqpListenerWorker::doWork() {
	CAF_CM_FUNCNAME("run");

	const std::string monitorDir = AppConfigUtils::getRequiredString("monitor_dir");
	const std::string listenerConfiguredStage2Path = FileSystemUtils::buildPath(
			monitorDir, "listenerConfiguredStage2.txt");
	if (FileSystemUtils::doesFileExist(listenerConfiguredStage2Path)) {
		uint32 intShutdownTimeout = AppConfigUtils::getOptionalUint32(
				"communication_amqp",
				"shutdown_timeout");
		intShutdownTimeout = std::max(intShutdownTimeout, static_cast<uint32>(5000));

		SmartPtrCIntegrationAppContext intAppContext;
		try {
			CLoggingUtils::setStartupConfigFile(
				AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogConfigFile),
				AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogDir));

			uint32 intStartupTimeout = AppConfigUtils::getOptionalUint32(
					"communication_amqp",
					"startup_timeout");
			intStartupTimeout = std::max(intStartupTimeout, static_cast<uint32>(5000));

			uint32 connectionRetryInterval = AppConfigUtils::getOptionalUint32(
					"communication_amqp",
					"connection_retry_interval");
			connectionRetryInterval = std::max(connectionRetryInterval, static_cast<uint32>(5000));

			bool isSignaled = false;
			do {
				try {
					CAF_CM_LOG_DEBUG_VA0("***** Initializing context");
					intAppContext.CreateInstance();
					intAppContext->initialize(
							intStartupTimeout,
							AppConfigUtils::getRequiredString("communication_amqp", "context_file"));
					CAF_CM_LOG_DEBUG_VA0("***** Started. Waiting for stop signal.");
					{
						CAF_THREADSIGNAL_LOCK_UNLOCK;
						_stopSignal.wait(CAF_THREADSIGNAL_MUTEX, 0);
					}
					CAF_CM_LOG_DEBUG_VA0("***** Received stop signal.");
					break;
				} catch (AmqpClient::AmqpExceptions::AmqpTimeoutException *ex) {
					_logger.log(
							log4cpp::Priority::WARN,
							_cm_funcName_,
							__LINE__,
							ex);
					ex->Release();
					CThreadUtils::sleep(connectionRetryInterval);
				}
				CAF_CM_CATCH_ALL;
				CAF_CM_THROWEXCEPTION;
				{
					CAF_THREADSIGNAL_LOCK_UNLOCK;
					isSignaled = _stopSignal.waitOrTimeout(CAF_THREADSIGNAL_MUTEX, 100);
				}
			} while (!isSignaled);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;

		try {
			if (intAppContext) {
				intAppContext->terminate(intShutdownTimeout);
			}
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_THROWEXCEPTION;
	} else {
		CAF_CM_LOG_WARN_VA0("Listener not configured");
	}
}

void AmqpListenerWorker::stopWork() {
	CAF_CM_FUNCNAME_VALIDATE("stop");
	CAF_CM_LOG_DEBUG_VA0("***** Setting stop signal.");
	_stopSignal.signal();
}
