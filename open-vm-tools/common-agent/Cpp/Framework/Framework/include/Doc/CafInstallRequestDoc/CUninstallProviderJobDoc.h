/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CUninstallProviderJobDoc_h_
#define CUninstallProviderJobDoc_h_

#include "Doc/CafInstallRequestDoc/CafInstallRequestDocTypes.h"

namespace Caf {

/// A simple container for objects of type UninstallProviderJob
class CAFINSTALLREQUESTDOC_LINKAGE CUninstallProviderJobDoc {
public:
	CUninstallProviderJobDoc();
	virtual ~CUninstallProviderJobDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const UUID jobId,
		const std::string providerNamespace,
		const std::string providerName,
		const std::string providerVersion,
		const PACKAGE_OS_TYPE packageOSType);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the JobId
	UUID getJobId() const;

	/// Accessor for the ProviderNamespace
	std::string getProviderNamespace() const;

	/// Accessor for the ProviderName
	std::string getProviderName() const;

	/// Accessor for the ProviderVersion
	std::string getProviderVersion() const;

	/// Accessor for the PackageOSType
	PACKAGE_OS_TYPE getPackageOSType() const;

private:
	UUID _clientId;
	UUID _jobId;
	std::string _providerNamespace;
	std::string _providerName;
	std::string _providerVersion;
	PACKAGE_OS_TYPE _packageOSType;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CUninstallProviderJobDoc);
};

CAF_DECLARE_SMART_POINTER(CUninstallProviderJobDoc);

}

#endif
