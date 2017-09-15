/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CProtocolDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"

using namespace Caf;

/// Set of protocol
CProtocolCollectionDoc::CProtocolCollectionDoc() :
	_isInitialized(false) {}
CProtocolCollectionDoc::~CProtocolCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProtocolCollectionDoc::initialize(
	const std::deque<SmartPtrCProtocolDoc> protocol) {
	if (! _isInitialized) {
		_protocol = protocol;

		_isInitialized = true;
	}
}

/// Used to change the logging level for a specific component
std::deque<SmartPtrCProtocolDoc> CProtocolCollectionDoc::getProtocol() const {
	return _protocol;
}






