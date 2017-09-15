/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderInfraDoc/CClassCollectionDoc.h"
#include "Doc/ProviderInfraDoc/CSchemaSummaryDoc.h"

using namespace Caf;

/// A simple container for objects of type SchemaSummary
CSchemaSummaryDoc::CSchemaSummaryDoc() :
	_isInitialized(false) {}
CSchemaSummaryDoc::~CSchemaSummaryDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CSchemaSummaryDoc::initialize(
	const std::string providerNamespace,
	const std::string providerName,
	const std::string providerVersion,
	const SmartPtrCClassCollectionDoc classCollection,
	const std::string invokerPath) {
	if (! _isInitialized) {
		_providerNamespace = providerNamespace;
		_providerName = providerName;
		_providerVersion = providerVersion;
		_classCollection = classCollection;
		_invokerPath = invokerPath;

		_isInitialized = true;
	}
}

/// Accessor for the ProviderNamespace
std::string CSchemaSummaryDoc::getProviderNamespace() const {
	return _providerNamespace;
}

/// Accessor for the ProviderName
std::string CSchemaSummaryDoc::getProviderName() const {
	return _providerName;
}

/// Accessor for the ProviderVersion
std::string CSchemaSummaryDoc::getProviderVersion() const {
	return _providerVersion;
}

/// Accessor for the ClassCollection
SmartPtrCClassCollectionDoc CSchemaSummaryDoc::getClassCollection() const {
	return _classCollection;
}

/// Accessor for the InvokerPath
std::string CSchemaSummaryDoc::getInvokerPath() const {
	return _invokerPath;
}






