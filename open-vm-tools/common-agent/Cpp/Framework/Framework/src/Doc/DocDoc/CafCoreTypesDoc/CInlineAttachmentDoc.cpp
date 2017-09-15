/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"

using namespace Caf;

/// A simple container for objects of type InlineAttachment
CInlineAttachmentDoc::CInlineAttachmentDoc() :
	_isInitialized(false) {}
CInlineAttachmentDoc::~CInlineAttachmentDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInlineAttachmentDoc::initialize(
	const std::string name,
	const std::string type,
	const std::string value) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_value = value;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CInlineAttachmentDoc::getName() const {
	return _name;
}

/// Accessor for the Type
std::string CInlineAttachmentDoc::getType() const {
	return _type;
}

/// Accessor for the Value
std::string CInlineAttachmentDoc::getValue() const {
	return _value;
}






