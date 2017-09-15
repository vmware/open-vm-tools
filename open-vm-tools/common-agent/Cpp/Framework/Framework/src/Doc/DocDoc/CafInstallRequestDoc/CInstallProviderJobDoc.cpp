/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"

#include "Doc/CafInstallRequestDoc/CFullPackageElemDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"

using namespace Caf;

/// A simple container for objects of type InstallProviderJob
CInstallProviderJobDoc::CInstallProviderJobDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_jobId(CAFCOMMON_GUID_NULL),
	_packageOSType(PACKAGE_OS_NONE),
	_isInitialized(false) {}
CInstallProviderJobDoc::~CInstallProviderJobDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstallProviderJobDoc::initialize(
	const UUID clientId,
	const UUID jobId,
	const std::string providerNamespace,
	const std::string providerName,
	const std::string providerVersion,
	const PACKAGE_OS_TYPE packageOSType,
	const std::deque<SmartPtrCFullPackageElemDoc> packageCollection) {
	if (! _isInitialized) {
		_clientId = clientId;
		_jobId = jobId;
		_providerNamespace = providerNamespace;
		_providerName = providerName;
		_providerVersion = providerVersion;
		_packageOSType = packageOSType;
		_packageCollection = packageCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CInstallProviderJobDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the JobId
UUID CInstallProviderJobDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the ProviderNamespace
std::string CInstallProviderJobDoc::getProviderNamespace() const {
	return _providerNamespace;
}

/// Accessor for the ProviderName
std::string CInstallProviderJobDoc::getProviderName() const {
	return _providerName;
}

/// Accessor for the ProviderVersion
std::string CInstallProviderJobDoc::getProviderVersion() const {
	return _providerVersion;
}

/// Accessor for the PackageOSType
PACKAGE_OS_TYPE CInstallProviderJobDoc::getPackageOSType() const {
	return _packageOSType;
}

/// Accessor for the PackageVal
std::deque<SmartPtrCFullPackageElemDoc> CInstallProviderJobDoc::getPackageCollection() const {
	return _packageCollection;
}





