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
#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/MgmtTypesDoc/CMgmtCollectInstancesDoc.h"

using namespace Caf;

/// A simple container for objects of type MgmtCollectInstances
CMgmtCollectInstancesDoc::CMgmtCollectInstancesDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CMgmtCollectInstancesDoc::~CMgmtCollectInstancesDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CMgmtCollectInstancesDoc::initialize(
	const UUID jobId,
	const SmartPtrCClassSpecifierDoc classSpecifier,
	const SmartPtrCParameterCollectionDoc parameterCollection) {
	if (! _isInitialized) {
		_jobId = jobId;
		_classSpecifier = classSpecifier;
		_parameterCollection = parameterCollection;

		_isInitialized = true;
	}
}

/// Accessor for the JobId
UUID CMgmtCollectInstancesDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the ClassSpecifier
SmartPtrCClassSpecifierDoc CMgmtCollectInstancesDoc::getClassSpecifier() const {
	return _classSpecifier;
}

/// Accessor for the ParameterCollection
SmartPtrCParameterCollectionDoc CMgmtCollectInstancesDoc::getParameterCollection() const {
	return _parameterCollection;
}





