/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAuthnAuthzDoc.h"
#include "Doc/CafCoreTypesDoc/CAuthnAuthzCollectionDoc.h"

using namespace Caf;

/// Set of logging levels for different components
CAuthnAuthzCollectionDoc::CAuthnAuthzCollectionDoc() :
	_isInitialized(false) {}
CAuthnAuthzCollectionDoc::~CAuthnAuthzCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAuthnAuthzCollectionDoc::initialize(
	const std::deque<SmartPtrCAuthnAuthzDoc> authnAuthz) {
	if (! _isInitialized) {
		_authnAuthz = authnAuthz;

		_isInitialized = true;
	}
}

/// Used to change the logging level for a specific component
std::deque<SmartPtrCAuthnAuthzDoc> CAuthnAuthzCollectionDoc::getAuthnAuthz() const {
	return _authnAuthz;
}






