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
#include "Doc/ResponseDoc/CEventManifestDoc.h"

using namespace Caf;

/// A simple container for objects of type EventManifest
CEventManifestDoc::CEventManifestDoc() :
	_isInitialized(false) {}
CEventManifestDoc::~CEventManifestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CEventManifestDoc::initialize(
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion,
	const std::string operationName,
	const SmartPtrCAttachmentCollectionDoc attachmentCollection) {
	if (! _isInitialized) {
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;
		_operationName = operationName;
		_attachmentCollection = attachmentCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClassNamespace
std::string CEventManifestDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CEventManifestDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CEventManifestDoc::getClassVersion() const {
	return _classVersion;
}

/// Accessor for the OperationName
std::string CEventManifestDoc::getOperationName() const {
	return _operationName;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CEventManifestDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}






