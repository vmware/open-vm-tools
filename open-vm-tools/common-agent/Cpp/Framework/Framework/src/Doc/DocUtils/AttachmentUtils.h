/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef AttachmentUtils_h_
#define AttachmentUtils_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"

namespace Caf {
class DOCUTILS_LINKAGE AttachmentUtils {
public:
	static SmartPtrCAttachmentDoc findOptionalAttachment(
		const std::string& attachmentName,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection);

	static SmartPtrCAttachmentDoc findRequiredAttachment(
		const std::string& attachmentName,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection);

private:
	CAF_CM_DECLARE_NOCREATE(AttachmentUtils);
};

}

#endif
