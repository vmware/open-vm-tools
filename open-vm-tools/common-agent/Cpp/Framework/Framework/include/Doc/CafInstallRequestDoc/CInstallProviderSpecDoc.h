/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstallProviderSpecDoc_h_
#define CInstallProviderSpecDoc_h_


#include "Doc/CafInstallRequestDoc/CMinPackageElemDoc.h"

namespace Caf {

/// A simple container for objects of type InstallProviderSpec
class CAFINSTALLREQUESTDOC_LINKAGE CInstallProviderSpecDoc {
public:
	CInstallProviderSpecDoc();
	virtual ~CInstallProviderSpecDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID clientId,
		const std::string providerNamespace,
		const std::string providerName,
		const std::string providerVersion,
		const std::deque<SmartPtrCMinPackageElemDoc> packageCollection);

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the ProviderNamespace
	std::string getProviderNamespace() const;

	/// Accessor for the ProviderName
	std::string getProviderName() const;

	/// Accessor for the ProviderVersion
	std::string getProviderVersion() const;

	/// Accessor for the PackageVal
	std::deque<SmartPtrCMinPackageElemDoc> getPackageCollection() const;

private:
	UUID _clientId;
	std::string _providerNamespace;
	std::string _providerName;
	std::string _providerVersion;
	std::deque<SmartPtrCMinPackageElemDoc> _packageCollection;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CInstallProviderSpecDoc);
};

CAF_DECLARE_SMART_POINTER(CInstallProviderSpecDoc);

}

#endif
