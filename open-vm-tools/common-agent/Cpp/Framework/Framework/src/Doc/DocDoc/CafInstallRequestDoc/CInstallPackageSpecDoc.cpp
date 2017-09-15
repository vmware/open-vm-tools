/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallPackageSpecDoc.h"

using namespace Caf;

/// A simple container for objects of type InstallPackageSpec
CInstallPackageSpecDoc::CInstallPackageSpecDoc() :
	_isInitialized(false) {}
CInstallPackageSpecDoc::~CInstallPackageSpecDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInstallPackageSpecDoc::initialize(
	const std::string packageNamespace,
	const std::string packageName,
	const std::string packageVersion,
	const std::string startupAttachmentName,
	const std::string packageAttachmentName,
	const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection,
	const SmartPtrCAttachmentCollectionDoc attachmentCollection,
	const std::string arguments) {
	if (! _isInitialized) {
		_packageNamespace = packageNamespace;
		_packageName = packageName;
		_packageVersion = packageVersion;
		_startupAttachmentName = startupAttachmentName;
		_packageAttachmentName = packageAttachmentName;
		_attachmentNameCollection = attachmentNameCollection;
		_attachmentCollection = attachmentCollection;
		_arguments = arguments;

		_isInitialized = true;
	}
}

/// Accessor for the PackageNamespace
std::string CInstallPackageSpecDoc::getPackageNamespace() const {
	return _packageNamespace;
}

/// Accessor for the PackageName
std::string CInstallPackageSpecDoc::getPackageName() const {
	return _packageName;
}

/// Accessor for the PackageVersion
std::string CInstallPackageSpecDoc::getPackageVersion() const {
	return _packageVersion;
}

/// Accessor for the StartupAttachmentName
std::string CInstallPackageSpecDoc::getStartupAttachmentName() const {
	return _startupAttachmentName;
}

/// Accessor for the PackageAttachmentName
std::string CInstallPackageSpecDoc::getPackageAttachmentName() const {
	return _packageAttachmentName;
}

/// Accessor for the AttachmentNameCollection
SmartPtrCAttachmentNameCollectionDoc CInstallPackageSpecDoc::getSupportingAttachmentNameCollection() const {
	return _attachmentNameCollection;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CInstallPackageSpecDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}

/// Accessor for the Arguments
std::string CInstallPackageSpecDoc::getArguments() const {
	return _arguments;
}






