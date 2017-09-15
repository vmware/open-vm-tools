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

using namespace Caf;

/// A simple container for objects of type MinPackageElem
CMinPackageElemDoc::CMinPackageElemDoc() :
	_index(0),
	_isInitialized(false) {}
CMinPackageElemDoc::~CMinPackageElemDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMinPackageElemDoc::initialize(
	const int32 index,
	const std::string packageNamespace,
	const std::string packageName,
	const std::string packageVersion) {
	if (! _isInitialized) {
		_index = index;
		_packageNamespace = packageNamespace;
		_packageName = packageName;
		_packageVersion = packageVersion;

		_isInitialized = true;
	}
}

/// Accessor for the Index
int32 CMinPackageElemDoc::getIndex() const {
	return _index;
}

/// Accessor for the PackageNamespace
std::string CMinPackageElemDoc::getPackageNamespace() const {
	return _packageNamespace;
}

/// Accessor for the PackageName
std::string CMinPackageElemDoc::getPackageName() const {
	return _packageName;
}

/// Accessor for the PackageVersion
std::string CMinPackageElemDoc::getPackageVersion() const {
	return _packageVersion;
}





