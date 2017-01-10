/*
 *	Copyright (C) 2004-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Common/CLoggingSetter.h"
#include "Common/CLoggingUtils.h"

using namespace Caf;

////////////////////////////////////////////////////////////////////////
//
//  CLoggingSetter::CLoggingSetter()
//
////////////////////////////////////////////////////////////////////////
CLoggingSetter::CLoggingSetter() :
		_isInitialized(false),
		_remapLoggingLocation(false),
	CAF_CM_INIT_LOG("CLoggingSetter") {
}

////////////////////////////////////////////////////////////////////////
//
//  CLoggingSetter::~CLoggingSetter()
//
////////////////////////////////////////////////////////////////////////
CLoggingSetter::~CLoggingSetter() {
	CAF_CM_FUNCNAME("~CLoggingSetter");

	try {
		if (_isInitialized) {
			if (_remapLoggingLocation) {
				CAF_CM_LOG_DEBUG_VA0("Resetting log config dir");
				CLoggingUtils::resetConfigFile();
				CAF_CM_LOG_DEBUG_VA0("Reset log config dir");
			}
		}
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_CLEAREXCEPTION;
}

////////////////////////////////////////////////////////////////////////
//
//  CLoggingSetter::Initialize()
//
////////////////////////////////////////////////////////////////////////
void CLoggingSetter::initialize(const std::string& logDir) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_STRING(logDir);

	_remapLoggingLocation = AppConfigUtils::getOptionalBoolean("remap_logging_location");
	if (_remapLoggingLocation) {
		CAF_CM_LOG_DEBUG_VA1("Setting log config dir - %s", logDir.c_str());
		CLoggingUtils::setLogDir(logDir);
		CAF_CM_LOG_DEBUG_VA1("Set log config dir - %s", logDir.c_str());
	}

	_isInitialized = true;
}
