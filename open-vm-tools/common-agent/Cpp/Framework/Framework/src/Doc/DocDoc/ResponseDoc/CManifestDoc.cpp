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
#include "Doc/ResponseDoc/CManifestDoc.h"

using namespace Caf;

/// A simple container for objects of type Manifest
CManifestDoc::CManifestDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CManifestDoc::~CManifestDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CManifestDoc::initialize(
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion,
	const UUID jobId,
	const std::string operationName,
	const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection) {
	if (! _isInitialized) {
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;
		_jobId = jobId;
		_operationName = operationName;
		_attachmentNameCollection = attachmentNameCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClassNamespace
std::string CManifestDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CManifestDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CManifestDoc::getClassVersion() const {
	return _classVersion;
}

/// Accessor for the JobId
UUID CManifestDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the OperationName
std::string CManifestDoc::getOperationName() const {
	return _operationName;
}

/// Accessor for the AttachmentNameCollection
SmartPtrCAttachmentNameCollectionDoc CManifestDoc::getAttachmentNameCollection() const {
	return _attachmentNameCollection;
}





