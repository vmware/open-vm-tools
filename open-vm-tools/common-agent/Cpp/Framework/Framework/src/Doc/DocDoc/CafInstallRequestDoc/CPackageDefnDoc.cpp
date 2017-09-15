/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CPackageDefnDoc.h"

using namespace Caf;

/// A simple container for objects of type PackageDefn
CPackageDefnDoc::CPackageDefnDoc() :
	_isInitialized(false) {}
CPackageDefnDoc::~CPackageDefnDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPackageDefnDoc::initialize(
	const std::string startupAttachmentName,
	const std::string packageAttachmentName,
	const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection,
	const std::string arguments) {
	if (! _isInitialized) {
		_startupAttachmentName = startupAttachmentName;
		_packageAttachmentName = packageAttachmentName;
		_attachmentNameCollection = attachmentNameCollection;
		_arguments = arguments;

		_isInitialized = true;
	}
}

/// Accessor for the StartupAttachmentName
std::string CPackageDefnDoc::getStartupAttachmentName() const {
	return _startupAttachmentName;
}

/// Accessor for the PackageAttachmentName
std::string CPackageDefnDoc::getPackageAttachmentName() const {
	return _packageAttachmentName;
}

/// Accessor for the AttachmentNameCollection
SmartPtrCAttachmentNameCollectionDoc CPackageDefnDoc::getSupportingAttachmentNameCollection() const {
	return _attachmentNameCollection;
}

/// Accessor for the Arguments
std::string CPackageDefnDoc::getArguments() const {
	return _arguments;
}






