/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef AttachmentUtils_h_
#define AttachmentUtils_h_

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
