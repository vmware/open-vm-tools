/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderSpecDoc.h"

using namespace Caf;

/// A simple container for objects of type InstallProviderSpec
CInstallProviderSpecDoc::CInstallProviderSpecDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CInstallProviderSpecDoc::~CInstallProviderSpecDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstallProviderSpecDoc::initialize(
	const UUID clientId,
	const std::string providerNamespace,
	const std::string providerName,
	const std::string providerVersion,
	const std::deque<SmartPtrCMinPackageElemDoc> packageCollection) {
	if (! _isInitialized) {
		_clientId = clientId;
		_providerNamespace = providerNamespace;
		_providerName = providerName;
		_providerVersion = providerVersion;
		_packageCollection = packageCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CInstallProviderSpecDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the ProviderNamespace
std::string CInstallProviderSpecDoc::getProviderNamespace() const {
	return _providerNamespace;
}

/// Accessor for the ProviderName
std::string CInstallProviderSpecDoc::getProviderName() const {
	return _providerName;
}

/// Accessor for the ProviderVersion
std::string CInstallProviderSpecDoc::getProviderVersion() const {
	return _providerVersion;
}

/// Accessor for the PackageVal
std::deque<SmartPtrCMinPackageElemDoc> CInstallProviderSpecDoc::getPackageCollection() const {
	return _packageCollection;
}





