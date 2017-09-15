/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPackageDefnDoc_h_
#define CPackageDefnDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type PackageDefn
class CAFINSTALLREQUESTDOC_LINKAGE CPackageDefnDoc {
public:
	CPackageDefnDoc();
	virtual ~CPackageDefnDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string startupAttachmentName,
		const std::string packageAttachmentName,
		const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection,
		const std::string arguments);

public:
	/// Accessor for the StartupAttachmentName
	std::string getStartupAttachmentName() const;

	/// Accessor for the PackageAttachmentName
	std::string getPackageAttachmentName() const;

	/// Accessor for the AttachmentNameCollection
	SmartPtrCAttachmentNameCollectionDoc getSupportingAttachmentNameCollection() const;

	/// Accessor for the Arguments
	std::string getArguments() const;

private:
	bool _isInitialized;

	std::string _startupAttachmentName;
	std::string _packageAttachmentName;
	SmartPtrCAttachmentNameCollectionDoc _attachmentNameCollection;
	std::string _arguments;

private:
	CAF_CM_DECLARE_NOCOPY(CPackageDefnDoc);
};

CAF_DECLARE_SMART_POINTER(CPackageDefnDoc);

}

#endif
