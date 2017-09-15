/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (c) 2010 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#include "stdafx.h"
#include "CConfigEnvReadingMessageSource.h"

using namespace Caf;

CConfigEnvReadingMessageSource::CConfigEnvReadingMessageSource() :
		_isInitialized(false),
		_refreshSec(0),
		_lastRefreshSec(0),
	CAF_CM_INIT_LOG("CConfigEnvReadingMessageSource") {
}

CConfigEnvReadingMessageSource::~CConfigEnvReadingMessageSource() {
}

void CConfigEnvReadingMessageSource::initialize(
		const SmartPtrIDocument& configSection,
		const SmartPtrIConfigEnv& configEnv) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	CAF_CM_VALIDATE_INTERFACE(configEnv);
	CAF_CM_VALIDATE_INTERFACE(configSection);

	_id = configSection->findRequiredAttribute("id");
	const SmartPtrIDocument pollerDoc = configSection->findOptionalChild("poller");

	_configEnv = configEnv;

	setPollerMetadata(pollerDoc);
	_refreshSec = 0;

	_isInitialized = true;
}

bool CConfigEnvReadingMessageSource::doSend(
		const SmartPtrIIntMessage&,
		int32) {
	CAF_CM_FUNCNAME("doSend");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	CAF_CM_EXCEPTIONEX_VA1(
			UnsupportedOperationException,
			E_NOTIMPL,
			"This is not a sending channel: %s", _id.c_str());
}

SmartPtrIIntMessage CConfigEnvReadingMessageSource::doReceive(
		const int32 timeout) {
	CAF_CM_FUNCNAME("receive");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	if (timeout > 0) {
		CAF_CM_EXCEPTIONEX_VA1(UnsupportedOperationException, E_INVALIDARG,
			"Timeout not currently supported: %s", _id.c_str());
	}

	SmartPtrIIntMessage message;
	const SmartPtrCPersistenceDoc persistence = _configEnv->getUpdated(0);
	if (! persistence.IsNull()) {
		SmartPtrCIntMessage messageImpl;
		messageImpl.CreateInstance();
		messageImpl->initializeStr(XmlRoots::savePersistenceToString(persistence),
				IIntMessage::SmartPtrCHeaders(), IIntMessage::SmartPtrCHeaders());
		message = messageImpl;
	}

	return message;
}

bool CConfigEnvReadingMessageSource::isRefreshNecessary(
		const uint32 refreshSec,
		const uint64 lastRefreshSec) const {
	bool rc = false;

	if (refreshSec == 0) {
		rc = true;
	} else {
		const uint64 currentTimeSec = getTimeSec();
		if ((currentTimeSec - lastRefreshSec) > refreshSec) {
			rc = true;
		}
	}

	return rc;
}

uint64 CConfigEnvReadingMessageSource::getTimeSec() const {
		GTimeVal curTime;
	::g_get_current_time(&curTime);
	uint64 rc = curTime.tv_sec;

	return rc;
}
