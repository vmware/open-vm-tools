/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CLocalSecurityDoc.h"
#include "Doc/PersistenceDoc/CPersistenceProtocolCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityCollectionDoc.h"
#include "Doc/PersistenceDoc/CPersistenceDoc.h"

using namespace Caf;

/// A simple container for objects of type PersistenceEnvelope
CPersistenceDoc::CPersistenceDoc() :
	_isInitialized(false) {}
CPersistenceDoc::~CPersistenceDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPersistenceDoc::initialize(
	const SmartPtrCLocalSecurityDoc& localSecurity,
	const SmartPtrCRemoteSecurityCollectionDoc& remoteSecurityCollection,
	const SmartPtrCPersistenceProtocolCollectionDoc& persistenceProtocolCollection,
	const std::string version) {
	if (! _isInitialized) {
		_localSecurity = localSecurity;
		_remoteSecurityCollection = remoteSecurityCollection;
		_persistenceProtocolCollection = persistenceProtocolCollection;
		_version = version;

		_isInitialized = true;
	}
}

/// Accessor for the LocalSecurity
SmartPtrCLocalSecurityDoc CPersistenceDoc::getLocalSecurity() const {
	return _localSecurity;
}

/// Accessor for the Protocol Collection
SmartPtrCRemoteSecurityCollectionDoc CPersistenceDoc::getRemoteSecurityCollection() const {
	return _remoteSecurityCollection;
}

/// Accessor for the PersistenceProtocol
SmartPtrCPersistenceProtocolCollectionDoc CPersistenceDoc::getPersistenceProtocolCollection() const {
	return _persistenceProtocolCollection;
}

/// Accessor for the version
std::string CPersistenceDoc::getVersion() const {
	return _version;
}






