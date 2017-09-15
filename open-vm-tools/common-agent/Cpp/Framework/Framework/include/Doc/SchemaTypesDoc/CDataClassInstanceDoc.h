/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CDataClassInstanceDoc_h_
#define CDataClassInstanceDoc_h_


#include "Doc/SchemaTypesDoc/CCmdlMetadataDoc.h"
#include "Doc/SchemaTypesDoc/CCmdlUnionDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassPropertyDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassSubInstanceDoc.h"

namespace Caf {

/// A simple container for objects of type DataClassInstance
class SCHEMATYPESDOC_LINKAGE CDataClassInstanceDoc {
public:
	CDataClassInstanceDoc();
	virtual ~CDataClassInstanceDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const std::deque<SmartPtrCCmdlMetadataDoc> cmdlMetadataCollection,
		const std::deque<SmartPtrCDataClassPropertyDoc> propertyCollection,
		const std::deque<SmartPtrCDataClassSubInstanceDoc> instancePropertyCollection,
		const SmartPtrCCmdlUnionDoc cmdlUnion);

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Accessor for the CmdlMetadata
	std::deque<SmartPtrCCmdlMetadataDoc> getCmdlMetadataCollection() const;

	/// Accessor for the Property
	std::deque<SmartPtrCDataClassPropertyDoc> getPropertyCollection() const;

	/// Accessor for the InstanceProperty
	std::deque<SmartPtrCDataClassSubInstanceDoc> getInstancePropertyCollection() const;

	/// Accessor for the CmdlUnion
	SmartPtrCCmdlUnionDoc getCmdlUnion() const;

private:
	bool _isInitialized;

	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	std::deque<SmartPtrCCmdlMetadataDoc> _cmdlMetadataCollection;
	std::deque<SmartPtrCDataClassPropertyDoc> _propertyCollection;
	std::deque<SmartPtrCDataClassSubInstanceDoc> _instancePropertyCollection;
	SmartPtrCCmdlUnionDoc _cmdlUnion;

private:
	CAF_CM_DECLARE_NOCOPY(CDataClassInstanceDoc);
};

CAF_DECLARE_SMART_POINTER(CDataClassInstanceDoc);

}

#endif
