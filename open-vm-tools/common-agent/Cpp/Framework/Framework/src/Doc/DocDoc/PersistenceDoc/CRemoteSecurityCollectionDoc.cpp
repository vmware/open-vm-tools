/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type RemoteSecurityCollection
CRemoteSecurityCollectionDoc::CRemoteSecurityCollectionDoc() :
	_isInitialized(false) {}
CRemoteSecurityCollectionDoc::~CRemoteSecurityCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRemoteSecurityCollectionDoc::initialize(
	const std::deque<SmartPtrCRemoteSecurityDoc> remoteSecurity) {
	if (! _isInitialized) {
		_remoteSecurity = remoteSecurity;

		_isInitialized = true;
	}
}

/// Accessor for the RemoteSecurity
std::deque<SmartPtrCRemoteSecurityDoc> CRemoteSecurityCollectionDoc::getRemoteSecurity() const {
	return _remoteSecurity;
}






