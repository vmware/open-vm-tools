/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/PersistenceDoc/CCertCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type CertCollection
CCertCollectionDoc::CCertCollectionDoc() :
	_isInitialized(false) {}
CCertCollectionDoc::~CCertCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CCertCollectionDoc::initialize(
	const std::deque<std::string> certCollection) {
	if (! _isInitialized) {
		_certCollection = certCollection;

		_isInitialized = true;
	}
}

/// Accessor for the Cert
std::deque<std::string> CCertCollectionDoc::getCert() const {
	return _certCollection;
}






