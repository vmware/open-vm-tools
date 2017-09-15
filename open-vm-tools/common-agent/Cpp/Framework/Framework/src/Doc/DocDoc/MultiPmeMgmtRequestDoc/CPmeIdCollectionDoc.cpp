/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/MultiPmeMgmtRequestDoc/CPmeIdCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type PmeIdCollection
CPmeIdCollectionDoc::CPmeIdCollectionDoc() :
	_isInitialized(false) {}
CPmeIdCollectionDoc::~CPmeIdCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPmeIdCollectionDoc::initialize(
	const std::deque<std::string> pmeIdCollection) {
	if (! _isInitialized) {
		_pmeIdCollection = pmeIdCollection;

		_isInitialized = true;
	}
}

/// Accessor for the PmeId
std::deque<std::string> CPmeIdCollectionDoc::getPmeIdCollection() const {
	return _pmeIdCollection;
}






