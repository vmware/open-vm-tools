/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Exception/CCafException.h"
#include "AttachmentUtils.h"

using namespace Caf;

SmartPtrCAttachmentDoc AttachmentUtils::findOptionalAttachment(
	const std::string& attachmentName,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("AttachmentUtils", "findOptionalAttachment");

	SmartPtrCAttachmentDoc attachmentRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(attachmentName);
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);

		const std::deque<SmartPtrCAttachmentDoc> attachmentCollectionInner =
			attachmentCollection->getAttachment();
		for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollectionInner);
			attachmentIter; attachmentIter++) {
			const SmartPtrCAttachmentDoc attachmentTmp = *attachmentIter;
			const std::string attachmentNameTmp = attachmentTmp->getName();
			if (attachmentNameTmp.compare(attachmentName) == 0) {
				attachmentRc = attachmentTmp;
			}
		}
	}
	CAF_CM_EXIT;

	return attachmentRc;
}

SmartPtrCAttachmentDoc AttachmentUtils::findRequiredAttachment(
	const std::string& attachmentName,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection) {
	CAF_CM_STATIC_FUNC_LOG("AttachmentUtils", "findRequiredAttachment");

	SmartPtrCAttachmentDoc attachmentRc;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_STRING(attachmentName);
		CAF_CM_VALIDATE_SMARTPTR(attachmentCollection);

		attachmentRc = findOptionalAttachment(attachmentName, attachmentCollection);
		if (attachmentRc.IsNull()) {
			CAF_CM_EXCEPTIONEX_VA1(NoSuchElementException, ERROR_NOT_FOUND,
				"Required attachment not found - %s", attachmentName.c_str());
		}
	}
	CAF_CM_EXIT;

	return attachmentRc;
}
