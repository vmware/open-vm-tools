/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type AttachmentCollection
CAttachmentCollectionDoc::CAttachmentCollectionDoc() :
	_isInitialized(false) {}
CAttachmentCollectionDoc::~CAttachmentCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAttachmentCollectionDoc::initialize(
	const std::deque<SmartPtrCAttachmentDoc> attachment,
	const std::deque<SmartPtrCInlineAttachmentDoc> inlineAttachment) {
	if (! _isInitialized) {
		_attachment = attachment;
		_inlineAttachment = inlineAttachment;

		_isInitialized = true;
	}
}

/// Accessor for the Attachment
std::deque<SmartPtrCAttachmentDoc> CAttachmentCollectionDoc::getAttachment() const {
	return _attachment;
}

/// Accessor for the InlineAttachment
std::deque<SmartPtrCInlineAttachmentDoc> CAttachmentCollectionDoc::getInlineAttachment() const {
	return _inlineAttachment;
}






