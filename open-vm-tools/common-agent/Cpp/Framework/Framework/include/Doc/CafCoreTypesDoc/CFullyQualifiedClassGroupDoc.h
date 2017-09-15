/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CFullyQualifiedClassGroupDoc_h_
#define CFullyQualifiedClassGroupDoc_h_

namespace Caf {

/// A simple container for objects of type FullyQualifiedClassGroup
class CAFCORETYPESDOC_LINKAGE CFullyQualifiedClassGroupDoc {
public:
	CFullyQualifiedClassGroupDoc();
	virtual ~CFullyQualifiedClassGroupDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string classNamespace,
		const std::string className,
		const std::string classVersion);

public:
	/// Accessor for the ClassNamespace
	std::string getClassNamespace() const;

	/// Accessor for the ClassName
	std::string getClassName() const;

	/// Accessor for the ClassVersion
	std::string getClassVersion() const;

private:
	bool _isInitialized;

	std::string _classNamespace;
	std::string _className;
	std::string _classVersion;

private:
	CAF_CM_DECLARE_NOCOPY(CFullyQualifiedClassGroupDoc);
};

CAF_DECLARE_SMART_POINTER(CFullyQualifiedClassGroupDoc);

}

#endif
