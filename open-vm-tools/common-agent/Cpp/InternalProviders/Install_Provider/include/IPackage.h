/*
 *	Author: bwilliams
 *	Created: July 25, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _InstallProvider_IPackage_h_
#define _InstallProvider_IPackage_h_


#include "ICafObject.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
IPackage : public ICafObject {
	CAF_DECL_UUID("e372d4bc-2384-4fcc-8042-f916f2d11bd2")

	virtual void fullPackageElem(
		const SmartPtrCAttachmentDoc packageAttachment,
		const std::string arguments,
		const SmartPtrCAttachmentCollectionDoc supportingAttachmentCollection,
		const std::string outputDir) = 0;

	virtual void unfullPackageElem(
		const SmartPtrCAttachmentDoc packageAttachment,
		const std::string arguments,
		const SmartPtrCAttachmentCollectionDoc supportingAttachmentCollection,
		const std::string outputDir) = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IPackage);

}

#endif
