/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/ProviderInfraDoc/CProviderRegDoc.h"

using namespace Caf;

/// A simple container for objects of type ProviderReg
CProviderRegDoc::CProviderRegDoc() :
	_staleSec(0),
	_isSchemaVisible(false),
	_isInitialized(false) {}
CProviderRegDoc::~CProviderRegDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CProviderRegDoc::initialize(
	const std::string providerNamespace,
	const std::string providerName,
	const std::string providerVersion,
	const int32 staleSec,
	const bool isSchemaVisible,
	const std::string invokerRelPath) {
	if (! _isInitialized) {
		_providerNamespace = providerNamespace;
		_providerName = providerName;
		_providerVersion = providerVersion;
		_staleSec = staleSec;
		_isSchemaVisible = isSchemaVisible;
		_invokerRelPath = invokerRelPath;

		_isInitialized = true;
	}
}

/// Accessor for the ProviderNamespace
std::string CProviderRegDoc::getProviderNamespace() const {
	return _providerNamespace;
}

/// Accessor for the ProviderName
std::string CProviderRegDoc::getProviderName() const {
	return _providerName;
}

/// Accessor for the ProviderVersion
std::string CProviderRegDoc::getProviderVersion() const {
	return _providerVersion;
}

/// Accessor for the StaleSec
int32 CProviderRegDoc::getStaleSec() const {
	return _staleSec;
}

/// Accessor for the IsSchemaVisible
bool CProviderRegDoc::getIsSchemaVisible() const {
	return _isSchemaVisible;
}

/// Accessor for the InvokerRelPath
std::string CProviderRegDoc::getInvokerRelPath() const {
	return _invokerRelPath;
}





