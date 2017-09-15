/*
 *  Author: bwilliams
 *  Created: Nov 16, 2015
 *
 *  Copyright (C) 2015-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/PersistenceDoc/CCertCollectionDoc.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"
#include "Doc/PersistenceDoc/CRemoteSecurityDoc.h"

using namespace Caf;

/// A simple container for objects of type CRemoteSecurityDoc
CRemoteSecurityDoc::CRemoteSecurityDoc() :
	_isInitialized(false) {}
CRemoteSecurityDoc::~CRemoteSecurityDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CRemoteSecurityDoc::initialize(
	const std::string& remoteId,
	const std::string& protocolName,
	const std::string& cmsCert,
	const std::string& cmsCipherName,
	const SmartPtrCCertCollectionDoc& cmsCertCollection,
	const std::string& cmsCertPath,
	const SmartPtrCCertPathCollectionDoc& cmsCertPathCollection) {
	if (! _isInitialized) {
		_remoteId = remoteId;
		_protocolName = protocolName;
		_cmsCert = cmsCert;
		_cmsCipherName = cmsCipherName;
		_cmsCertCollection = cmsCertCollection;

		_cmsCertPath = cmsCertPath;
		_cmsCertPathCollection = cmsCertPathCollection;

		_isInitialized = true;
	}
}

/// Accessor for the RemoteId
std::string CRemoteSecurityDoc::getRemoteId() const {
	return _remoteId;
}

/// Accessor for the ProtocolName
std::string CRemoteSecurityDoc::getProtocolName() const {
	return _protocolName;
}

/// Accessor for the cmsCert
std::string CRemoteSecurityDoc::getCmsCert() const {
	return _cmsCert;
}

/// Accessor for the CmsCipher
std::string CRemoteSecurityDoc::getCmsCipherName() const {
	return _cmsCipherName;
}

/// Accessor for the CertCollection
SmartPtrCCertCollectionDoc CRemoteSecurityDoc::getCmsCertCollection() const {
	return _cmsCertCollection;
}

/// Accessor for the cmsCertPath
std::string CRemoteSecurityDoc::getCmsCertPath() const {
	return _cmsCertPath;
}

/// Accessor for the CertPathCollection
SmartPtrCCertPathCollectionDoc CRemoteSecurityDoc::getCmsCertPathCollection() const {
	return _cmsCertPathCollection;
}







