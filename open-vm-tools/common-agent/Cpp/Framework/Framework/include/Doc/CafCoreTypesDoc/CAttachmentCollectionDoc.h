/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CAttachmentCollectionDoc_h_
#define CAttachmentCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"

namespace Caf {

/// A simple container for objects of type AttachmentCollection
class CAFCORETYPESDOC_LINKAGE CAttachmentCollectionDoc {
public:
	CAttachmentCollectionDoc();
	virtual ~CAttachmentCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCAttachmentDoc> attachment = std::deque<SmartPtrCAttachmentDoc>(),
		const std::deque<SmartPtrCInlineAttachmentDoc> inlineAttachment = std::deque<SmartPtrCInlineAttachmentDoc>());

public:
	/// Accessor for the Attachment
	std::deque<SmartPtrCAttachmentDoc> getAttachment() const;

	/// Accessor for the InlineAttachment
	std::deque<SmartPtrCInlineAttachmentDoc> getInlineAttachment() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCAttachmentDoc> _attachment;
	std::deque<SmartPtrCInlineAttachmentDoc> _inlineAttachment;

private:
	CAF_CM_DECLARE_NOCOPY(CAttachmentCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CAttachmentCollectionDoc);

}

#endif
