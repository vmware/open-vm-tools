/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/COperationDoc.h"
#include "Doc/ProviderRequestDoc/CProviderInvokeOperationDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderInvokeOperation
CProviderInvokeOperationDoc::CProviderInvokeOperationDoc() :
	_jobId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CProviderInvokeOperationDoc::~CProviderInvokeOperationDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderInvokeOperationDoc::initialize(
	const std::string classNamespace,
	const std::string className,
	const std::string classVersion,
	const UUID jobId,
	const std::string outputDir,
	const SmartPtrCOperationDoc operation) {
	if (! _isInitialized) {
		_classNamespace = classNamespace;
		_className = className;
		_classVersion = classVersion;
		_jobId = jobId;
		_outputDir = outputDir;
		_operation = operation;

		_isInitialized = true;
	}
}

/// Accessor for the ClassNamespace
std::string CProviderInvokeOperationDoc::getClassNamespace() const {
	return _classNamespace;
}

/// Accessor for the ClassName
std::string CProviderInvokeOperationDoc::getClassName() const {
	return _className;
}

/// Accessor for the ClassVersion
std::string CProviderInvokeOperationDoc::getClassVersion() const {
	return _classVersion;
}

/// Accessor for the JobId
UUID CProviderInvokeOperationDoc::getJobId() const {
	return _jobId;
}

/// Accessor for the OutputDir
std::string CProviderInvokeOperationDoc::getOutputDir() const {
	return _outputDir;
}

/// Accessor for the Operation
SmartPtrCOperationDoc CProviderInvokeOperationDoc::getOperation() const {
	return _operation;
}





