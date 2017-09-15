/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CPersistenceProtocolDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type PersistenceProtocolCollection
CPersistenceProtocolCollectionDoc::CPersistenceProtocolCollectionDoc() :
	_isInitialized(false) {}
CPersistenceProtocolCollectionDoc::~CPersistenceProtocolCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPersistenceProtocolCollectionDoc::initialize(
	const std::deque<SmartPtrCPersistenceProtocolDoc> persistenceProtocol) {
	if (! _isInitialized) {
		_persistenceProtocol = persistenceProtocol;

		_isInitialized = true;
	}
}

/// Accessor for the PersistenceProtocol
std::deque<SmartPtrCPersistenceProtocolDoc> CPersistenceProtocolCollectionDoc::getPersistenceProtocol() const {
	return _persistenceProtocol;
}






