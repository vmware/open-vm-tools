/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CMinPackageElemDoc_h_
#define CMinPackageElemDoc_h_

namespace Caf {

/// A simple container for objects of type MinPackageElem
class CAFINSTALLREQUESTDOC_LINKAGE CMinPackageElemDoc {
public:
	CMinPackageElemDoc();
	virtual ~CMinPackageElemDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const int32 index,
		const std::string packageNamespace,
		const std::string packageName,
		const std::string packageVersion);

public:
	/// Accessor for the Index
	int32 getIndex() const;

	/// Accessor for the PackageNamespace
	std::string getPackageNamespace() const;

	/// Accessor for the PackageName
	std::string getPackageName() const;

	/// Accessor for the PackageVersion
	std::string getPackageVersion() const;

private:
	int32 _index;
	std::string _packageNamespace;
	std::string _packageName;
	std::string _packageVersion;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CMinPackageElemDoc);
};

CAF_DECLARE_SMART_POINTER(CMinPackageElemDoc);

}

#endif
