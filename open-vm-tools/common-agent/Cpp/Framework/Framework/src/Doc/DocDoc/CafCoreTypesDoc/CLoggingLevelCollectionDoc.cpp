/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CLoggingLevelElemDoc.h"
#include "Doc/CafCoreTypesDoc/CLoggingLevelCollectionDoc.h"

using namespace Caf;

/// Set of logging levels for different components
CLoggingLevelCollectionDoc::CLoggingLevelCollectionDoc() :
	_isInitialized(false) {}
CLoggingLevelCollectionDoc::~CLoggingLevelCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CLoggingLevelCollectionDoc::initialize(
	const std::deque<SmartPtrCLoggingLevelElemDoc> loggingLevel) {
	if (! _isInitialized) {
		_loggingLevel = loggingLevel;

		_isInitialized = true;
	}
}

/// Used to change the logging level for a specific component
std::deque<SmartPtrCLoggingLevelElemDoc> CLoggingLevelCollectionDoc::getLoggingLevel() const {
	return _loggingLevel;
}






