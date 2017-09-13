/*
 *  Author: bwilliams
 *  Created: June 29, 2012
 *
 *	Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CManagementAgentHostWork.h"

using namespace Caf;

CManagementAgentHostWork::CManagementAgentHostWork() :
	_isInitialized(false),
	_isWorking(false),
	CAF_CM_INIT_LOG("CManagementAgentHostWork") {
}

CManagementAgentHostWork::~CManagementAgentHostWork() {
}

void CManagementAgentHostWork::initialize() {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

void CManagementAgentHostWork::doWork() {
	CAF_CM_FUNCNAME("doWork");

	SmartPtrCIntegrationAppContext integrationAppContext;

	uint32 hostIntegrationTimeoutMs = 5000;
	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		_isWorking = true;

		CLoggingUtils::setStartupConfigFile(
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogConfigFile));
		CLoggingUtils::setLogDir(
			AppConfigUtils::getRequiredString(_sAppConfigGlobalParamLogDir));

		const uint32 hostDelaySec = AppConfigUtils::getRequiredUint32(
			_sManagementAgentArea, "host_delay_sec");
		hostIntegrationTimeoutMs = AppConfigUtils::getRequiredUint32(
			_sManagementAgentArea, "host_integration_timeout_ms");

		integrationAppContext.CreateInstance();
		integrationAppContext->initialize(hostIntegrationTimeoutMs);

		while (_isWorking) {
			CThreadUtils::sleep(hostDelaySec * 1000);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;

	try {
		if (! integrationAppContext.IsNull()) {
			integrationAppContext->terminate(hostIntegrationTimeoutMs);
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_THROWEXCEPTION;
}

void CManagementAgentHostWork::stopWork() {
	CAF_CM_FUNCNAME_VALIDATE("stopWork");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		_isWorking = false;
	}
	CAF_CM_EXIT;
}
