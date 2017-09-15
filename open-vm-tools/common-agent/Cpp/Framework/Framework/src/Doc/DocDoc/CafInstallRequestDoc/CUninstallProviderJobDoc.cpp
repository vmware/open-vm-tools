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
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"

using namespace Caf;

/// A simple container for objects of type UninstallProviderJob
CUninstallProviderJobDoc::CUninstallProviderJobDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_jobId(CAFCOMMON_GUID_NULL),
	_packageOSType(PACKAGE_OS_NONE),
	_isInitialized(false) {}
CUninstallProviderJobDoc::~CUninstallProviderJobDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CUninstallProviderJobDoc::initialize(
	const UUID clientId,
	const UUID jobId,
	const std::string providerNamespace,
	const std::string providerName,
	const std::string providerVersion,
	const PACKAGE_OS_TYPE packageOSType) {
	if (! _isInitialized) {
		_clientId = clientId;
		_jobId = jobId;
		_providerNamespace = providerNamespace;
		_providerName = providerName;
		_providerVersion = providerVersion;
		_packageOSType = packageOSType;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CUninstallProviderJobDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the JobId
UUID CUninstallProviderJobDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the ProviderNamespace
std::string CUninstallProviderJobDoc::getProviderNamespace() const {
	return _providerNamespace;
}

/// Accessor for the ProviderName
std::string CUninstallProviderJobDoc::getProviderName() const {
	return _providerName;
}

/// Accessor for the ProviderVersion
std::string CUninstallProviderJobDoc::getProviderVersion() const {
	return _providerVersion;
}

/// Accessor for the PackageOSType
PACKAGE_OS_TYPE CUninstallProviderJobDoc::getPackageOSType() const {
	return _packageOSType;
}





