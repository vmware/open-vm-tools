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
#include "Doc/CafCoreTypesDoc/CInlineAttachmentCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type InlineAttachmentCollection
CInlineAttachmentCollectionDoc::CInlineAttachmentCollectionDoc() :
	_isInitialized(false) {}
CInlineAttachmentCollectionDoc::~CInlineAttachmentCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CInlineAttachmentCollectionDoc::initialize(
	const std::deque<SmartPtrCInlineAttachmentDoc> attachment) {
	if (! _isInitialized) {
		_attachment = attachment;

		_isInitialized = true;
	}
}

/// Accessor for the InlineAttachment
std::deque<SmartPtrCInlineAttachmentDoc> CInlineAttachmentCollectionDoc::getInlineAttachment() const {
	return _attachment;
}






