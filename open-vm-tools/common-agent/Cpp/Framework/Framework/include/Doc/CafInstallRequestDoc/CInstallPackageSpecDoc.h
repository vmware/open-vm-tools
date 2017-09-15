/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CInstallPackageSpecDoc_h_
#define CInstallPackageSpecDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type InstallPackageSpec
class CAFINSTALLREQUESTDOC_LINKAGE CInstallPackageSpecDoc {
public:
	CInstallPackageSpecDoc();
	virtual ~CInstallPackageSpecDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string packageNamespace,
		const std::string packageName,
		const std::string packageVersion,
		const std::string startupAttachmentName,
		const std::string packageAttachmentName,
		const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection,
		const SmartPtrCAttachmentCollectionDoc attachmentCollection,
		const std::string arguments);

public:
	/// Accessor for the PackageNamespace
	std::string getPackageNamespace() const;

	/// Accessor for the PackageName
	std::string getPackageName() const;

	/// Accessor for the PackageVersion
	std::string getPackageVersion() const;

	/// Accessor for the StartupAttachmentName
	std::string getStartupAttachmentName() const;

	/// Accessor for the PackageAttachmentName
	std::string getPackageAttachmentName() const;

	/// Accessor for the AttachmentNameCollection
	SmartPtrCAttachmentNameCollectionDoc getSupportingAttachmentNameCollection() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

	/// Accessor for the Arguments
	std::string getArguments() const;

private:
	bool _isInitialized;

	std::string _packageNamespace;
	std::string _packageName;
	std::string _packageVersion;
	std::string _startupAttachmentName;
	std::string _packageAttachmentName;
	SmartPtrCAttachmentNameCollectionDoc _attachmentNameCollection;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	std::string _arguments;

private:
	CAF_CM_DECLARE_NOCOPY(CInstallPackageSpecDoc);
};

CAF_DECLARE_SMART_POINTER(CInstallPackageSpecDoc);

}

#endif
