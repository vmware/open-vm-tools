/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/PersistenceDoc/CCertPathCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type CertPathCollection
CCertPathCollectionDoc::CCertPathCollectionDoc() :
	_isInitialized(false) {}
CCertPathCollectionDoc::~CCertPathCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CCertPathCollectionDoc::initialize(
	const std::deque<std::string> certPathCollection) {
	if (! _isInitialized) {
		_certPathCollection = certPathCollection;

		_isInitialized = true;
	}
}

/// Accessor for the Cert
std::deque<std::string> CCertPathCollectionDoc::getCertPath() const {
	return _certPathCollection;
}






