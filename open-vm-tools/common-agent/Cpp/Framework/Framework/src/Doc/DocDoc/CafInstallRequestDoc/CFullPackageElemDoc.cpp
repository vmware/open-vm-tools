/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafInstallRequestDoc/CPackageDefnDoc.h"
#include "Doc/CafInstallRequestDoc/CFullPackageElemDoc.h"

using namespace Caf;

/// A simple container for objects of type FullPackageElem
CFullPackageElemDoc::CFullPackageElemDoc() :
	_index(0),
	_isInitialized(false) {}
CFullPackageElemDoc::~CFullPackageElemDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CFullPackageElemDoc::initialize(
	const int32 index,
	const std::string packageNamespace,
	const std::string packageName,
	const std::string packageVersion,
	const SmartPtrCPackageDefnDoc installPackage,
	const SmartPtrCPackageDefnDoc uninstallPackage) {
	if (! _isInitialized) {
		_index = index;
		_packageNamespace = packageNamespace;
		_packageName = packageName;
		_packageVersion = packageVersion;
		_installPackage = installPackage;
		_uninstallPackage = uninstallPackage;

		_isInitialized = true;
	}
}

/// Accessor for the Index
int32 CFullPackageElemDoc::getIndex() const {
	return _index;
}

/// Accessor for the PackageNamespace
std::string CFullPackageElemDoc::getPackageNamespace() const {
	return _packageNamespace;
}

/// Accessor for the PackageName
std::string CFullPackageElemDoc::getPackageName() const {
	return _packageName;
}

/// Accessor for the PackageVersion
std::string CFullPackageElemDoc::getPackageVersion() const {
	return _packageVersion;
}

/// Accessor for the InstallPackage
SmartPtrCPackageDefnDoc CFullPackageElemDoc::getInstallPackage() const {
	return _installPackage;
}

/// Accessor for the UninstallPackage
SmartPtrCPackageDefnDoc CFullPackageElemDoc::getUninstallPackage() const {
	return _uninstallPackage;
}





