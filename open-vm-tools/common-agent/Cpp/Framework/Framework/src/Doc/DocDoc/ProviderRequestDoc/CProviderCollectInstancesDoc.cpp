/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CParameterCollectionDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectInstancesDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderCollectInstances
CProviderCollectInstancesDoc::CProviderCollectInstancesDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CProviderCollectInstancesDoc::~CProviderCollectInstancesDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderCollectInstancesDoc::initialize(
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion,
	const UUID jobId,
	const std::string outputDir,
	const SmartPtrCParameterCollectionDoc parameterCollection) {
	if (! _isInitialized) {
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;
		_jobId = jobId;
		_outputDir = outputDir;
		_parameterCollection = parameterCollection;

		_isInitialized = true;
	}
}

/// Accessor for the ClassNamespace
std::string CProviderCollectInstancesDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CProviderCollectInstancesDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CProviderCollectInstancesDoc::getClassVersion() const {
	return _classVersion;
}

/// Accessor for the JobId
UUID CProviderCollectInstancesDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the OutputDir
std::string CProviderCollectInstancesDoc::getOutputDir() const {
	return _outputDir;
}

/// Accessor for the ParameterCollection
SmartPtrCParameterCollectionDoc CProviderCollectInstancesDoc::getParameterCollection() const {
	return _parameterCollection;
}





