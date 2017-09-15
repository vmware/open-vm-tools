/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CClassSpecifierDoc.h"
#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtInvokeOperationDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtInvokeOperation
CMgmtInvokeOperationDoc::CMgmtInvokeOperationDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CMgmtInvokeOperationDoc::~CMgmtInvokeOperationDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtInvokeOperationDoc::initialize(
	const UUID jobId,
	const SmartPtrCClassSpecifierDoc classSpecifier,
	const SmartPtrCOperationDoc operation) {
	if (! _isInitialized) {
		_jobId = jobId;
		_classSpecifier = classSpecifier;
		_operation = operation;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CMgmtInvokeOperationDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the ClassSpecifier
SmartPtrCClassSpecifierDoc CMgmtInvokeOperationDoc::getClassSpecifier() const {
	return _classSpecifier;
}

/// Accessor for the Operation
SmartPtrCOperationDoc CMgmtInvokeOperationDoc::getOperation() const {
	return _operation;
}





