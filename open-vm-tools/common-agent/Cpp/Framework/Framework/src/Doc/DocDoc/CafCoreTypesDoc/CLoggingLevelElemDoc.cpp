/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CLoggingLevelElemDoc.h"

using namespace Caf;

/// Logging level for a component
CLoggingLevelElemDoc::CLoggingLevelElemDoc() :
	_component(LOGGINGCOMPONENT_NONE),
	_level(LOGGINGLEVEL_NONE),
	_isInitialized(false) {}
CLoggingLevelElemDoc::~CLoggingLevelElemDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CLoggingLevelElemDoc::initialize(
	const LOGGINGCOMPONENT_TYPE component,
	const LOGGINGLEVEL_TYPE level) {
	if (! _isInitialized) {
		_component = component;
		_level = level;

		_isInitialized = true;
	}
}

/// The logging level applies to this component
LOGGINGCOMPONENT_TYPE CLoggingLevelElemDoc::getComponent() const {
	return _component;
}

/// Set the logging level to this value
LOGGINGLEVEL_TYPE CLoggingLevelElemDoc::getLevel() const {
	return _level;
}

LOGGINGCOMPONENT_TYPE _component;
LOGGINGLEVEL_TYPE _level;





