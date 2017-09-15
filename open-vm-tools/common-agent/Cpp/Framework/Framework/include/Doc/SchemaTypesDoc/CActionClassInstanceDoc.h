/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CActionClassInstanceDoc_h_
#define CActionClassInstanceDoc_h_


#include "Doc/SchemaTypesDoc/CInstanceOperationCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type ActionClassInstance
class SCHEMATYPESDOC_LINKAGE CActionClassInstanceDoc {
public:
	CActionClassInstanceDoc();
	virtual ~CActionClassInstanceDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const SmartPtrCInstanceOperationCollectionDoc instanceOperationCollection);

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Accessor for the InstanceOperationCollection
	SmartPtrCInstanceOperationCollectionDoc getInstanceOperationCollection() const;

private:
	bool _isInitialized;

	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	SmartPtrCInstanceOperationCollectionDoc _instanceOperationCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CActionClassInstanceDoc);
};

CAF_DECLARE_SMART_POINTER(CActionClassInstanceDoc);

}

#endif
