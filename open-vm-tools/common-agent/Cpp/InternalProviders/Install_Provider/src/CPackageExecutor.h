/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CPackageExecutor_h_
#define CPackageExecutor_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"

namespace Caf {

/// Utilities used in the install process.
class CPackageExecutor {
public:
	static void executePackage(
		const SmartPtrCAttachmentDoc& startupAttachment,
		const std::string& startupArgument,
		const SmartPtrCAttachmentDoc& packageAttachment,
		const std::string& packageArguments,
		const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
		const std::string& outputDir);

private:
	static std::string createInstallInvoker(
		const SmartPtrCAttachmentDoc& startupAttachment,
		const std::string& startupArgument,
		const SmartPtrCAttachmentDoc& packageAttachment,
		const std::string& packageArguments,
		const SmartPtrCAttachmentCollectionDoc& supportingAttachmentCollection,
		const std::string& outputDir);

	static void runInstallInvoker(
		const std::string& installInvoker,
		const std::string& outputDir);

private:
	CAF_CM_DECLARE_NOCREATE(CPackageExecutor);
};

}

#endif // #ifndef CPackageExecutor_h_
