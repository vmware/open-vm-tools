/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstallBatchDoc_h_
#define CInstallBatchDoc_h_


#include "Doc/CafInstallRequestDoc/CGetInventoryJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"

namespace Caf {

/// A simple container for objects of type InstallBatch
class CAFINSTALLREQUESTDOC_LINKAGE CInstallBatchDoc {
public:
	CInstallBatchDoc();
	virtual ~CInstallBatchDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCGetInventoryJobDoc getInventory,
		const SmartPtrCInstallProviderJobDoc installProvider,
		const SmartPtrCUninstallProviderJobDoc uninstallProvider);

public:
	/// Accessor for the GetInventory
	SmartPtrCGetInventoryJobDoc getGetInventory() const;

	/// Accessor for the InstallProvider
	SmartPtrCInstallProviderJobDoc getInstallProvider() const;

	/// Accessor for the UninstallProvider
	SmartPtrCUninstallProviderJobDoc getUninstallProvider() const;

private:
	bool _isInitialized;

	SmartPtrCGetInventoryJobDoc _getInventory;
	SmartPtrCInstallProviderJobDoc _installProvider;
	SmartPtrCUninstallProviderJobDoc _uninstallProvider;

private:
	CAF_CM_DECLARE_NOCOPY(CInstallBatchDoc);
};

CAF_DECLARE_SMART_POINTER(CInstallBatchDoc);

}

#endif
