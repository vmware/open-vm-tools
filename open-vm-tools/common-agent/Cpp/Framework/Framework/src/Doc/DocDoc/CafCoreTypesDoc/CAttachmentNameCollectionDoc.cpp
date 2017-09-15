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

using namespace Caf;

/// A simple container for objects of type AttachmentNameCollection
CAttachmentNameCollectionDoc::CAttachmentNameCollectionDoc() :
	_isInitialized(false) {}
CAttachmentNameCollectionDoc::~CAttachmentNameCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAttachmentNameCollectionDoc::initialize(
	const std::deque<std::string> name) {
	if (! _isInitialized) {
		_name = name;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::deque<std::string> CAttachmentNameCollectionDoc::getName() const {
	return _name;
}






