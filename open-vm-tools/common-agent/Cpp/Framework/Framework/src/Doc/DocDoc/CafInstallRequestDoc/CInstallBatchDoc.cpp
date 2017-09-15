/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafInstallRequestDoc/CGetInventoryJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallBatchDoc.h"

using namespace Caf;

/// A simple container for objects of type InstallBatch
CInstallBatchDoc::CInstallBatchDoc() :
	_isInitialized(false) {}
CInstallBatchDoc::~CInstallBatchDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstallBatchDoc::initialize(
	const SmartPtrCGetInventoryJobDoc getInventory,
	const SmartPtrCInstallProviderJobDoc installProvider,
	const SmartPtrCUninstallProviderJobDoc uninstallProvider) {
	if (! _isInitialized) {
		_getInventory = getInventory;
		_installProvider = installProvider;
		_uninstallProvider = uninstallProvider;

		_isInitialized = true;
	}
}

/// Accessor for the GetInventory
SmartPtrCGetInventoryJobDoc CInstallBatchDoc::getGetInventory() const {
	return _getInventory;
}

/// Accessor for the InstallProvider
SmartPtrCInstallProviderJobDoc CInstallBatchDoc::getInstallProvider() const {
	return _installProvider;
}

/// Accessor for the UninstallProvider
SmartPtrCUninstallProviderJobDoc CInstallBatchDoc::getUninstallProvider() const {
	return _uninstallProvider;
}






