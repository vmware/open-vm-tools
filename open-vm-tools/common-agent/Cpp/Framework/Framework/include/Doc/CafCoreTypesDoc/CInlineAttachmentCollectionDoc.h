/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInlineAttachmentCollectionDoc_h_
#define CInlineAttachmentCollectionDoc_h_


#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"

namespace Caf {

/// A simple container for objects of type InlineAttachmentCollection
class CAFCORETYPESDOC_LINKAGE CInlineAttachmentCollectionDoc {
public:
	CInlineAttachmentCollectionDoc();
	virtual ~CInlineAttachmentCollectionDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::deque<SmartPtrCInlineAttachmentDoc> attachment = std::deque<SmartPtrCInlineAttachmentDoc>());

public:
	/// Accessor for the InlineAttachment
	std::deque<SmartPtrCInlineAttachmentDoc> getInlineAttachment() const;

private:
	bool _isInitialized;

	std::deque<SmartPtrCInlineAttachmentDoc> _attachment;

private:
	CAF_CM_DECLARE_NOCOPY(CInlineAttachmentCollectionDoc);
};

CAF_DECLARE_SMART_POINTER(CInlineAttachmentCollectionDoc);

}

#endif
