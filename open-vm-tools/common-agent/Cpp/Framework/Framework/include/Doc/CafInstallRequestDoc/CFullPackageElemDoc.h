/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CFullPackageElemDoc_h_
#define CFullPackageElemDoc_h_


#include "Doc/CafInstallRequestDoc/CPackageDefnDoc.h"

namespace Caf {

/// A simple container for objects of type FullPackageElem
class CAFINSTALLREQUESTDOC_LINKAGE CFullPackageElemDoc {
public:
	CFullPackageElemDoc();
	virtual ~CFullPackageElemDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const int32 index,
		const std::string packageNamespace,
		const std::string packageName,
		const std::string packageVersion,
		const SmartPtrCPackageDefnDoc installPackage,
		const SmartPtrCPackageDefnDoc uninstallPackage);

public:
	/// Accessor for the Index
	int32 getIndex() const;

	/// Accessor for the PackageNamespace
	std::string getPackageNamespace() const;

	/// Accessor for the PackageName
	std::string getPackageName() const;

	/// Accessor for the PackageVersion
	std::string getPackageVersion() const;

	/// Accessor for the InstallPackage
	SmartPtrCPackageDefnDoc getInstallPackage() const;

	/// Accessor for the UninstallPackage
	SmartPtrCPackageDefnDoc getUninstallPackage() const;

private:
	int32 _index;
	std::string _packageNamespace;
	std::string _packageName;
	std::string _packageVersion;
	SmartPtrCPackageDefnDoc _installPackage;
	SmartPtrCPackageDefnDoc _uninstallPackage;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CFullPackageElemDoc);
};

CAF_DECLARE_SMART_POINTER(CFullPackageElemDoc);

}

#endif
