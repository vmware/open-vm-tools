/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ResponseDoc/CEventKeyDoc.h"
#include "Doc/ResponseDoc/CEventKeyCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type EventKeyCollection
CEventKeyCollectionDoc::CEventKeyCollectionDoc() :
	_isInitialized(false) {}
CEventKeyCollectionDoc::~CEventKeyCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CEventKeyCollectionDoc::initialize(
	const std::deque<SmartPtrCEventKeyDoc> eventKey) {
	if (! _isInitialized) {
		_eventKey = eventKey;

		_isInitialized = true;
	}
}

/// Accessor for the EventKey
std::deque<SmartPtrCEventKeyDoc> CEventKeyCollectionDoc::getEventKey() const {
	return _eventKey;
}






