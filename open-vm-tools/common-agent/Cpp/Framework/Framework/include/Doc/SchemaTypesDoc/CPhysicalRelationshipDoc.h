/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CPhysicalRelationshipDoc_h_
#define CPhysicalRelationshipDoc_h_


#include "Doc/SchemaTypesDoc/CClassCardinalityDoc.h"
#include "Doc/SchemaTypesDoc/SchemaTypesDocTypes.h"

namespace Caf {

/// Describes a relationship between dataclass where the key information from data class instances comprising the relationship are listed in an instance of this class
class SCHEMATYPESDOC_LINKAGE CPhysicalRelationshipDoc {
public:
	CPhysicalRelationshipDoc();
	virtual ~CPhysicalRelationshipDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string namespaceVal,
		const std::string name,
		const std::string version,
		const ARITY_TYPE arity,
		const SmartPtrCClassCardinalityDoc dataClassLeft,
		const SmartPtrCClassCardinalityDoc dataClassRight,
		const std::string description = std::string());

public:
	/// Accessor for the NamespaceVal
	std::string getNamespaceVal() const;

	/// Accessor for the Name
	std::string getName() const;

	/// Accessor for the Version
	std::string getVersion() const;

	/// Accessor for the Arity
	ARITY_TYPE getArity() const;

	/// Accessor for the DataClassLeft
	SmartPtrCClassCardinalityDoc getDataClassLeft() const;

	/// Accessor for the DataClassRight
	SmartPtrCClassCardinalityDoc getDataClassRight() const;

	/// Accessor for the Description
	std::string getDescription() const;

private:
	std::string _namespaceVal;
	std::string _name;
	std::string _version;
	ARITY_TYPE _arity;
	SmartPtrCClassCardinalityDoc _dataClassLeft;
	SmartPtrCClassCardinalityDoc _dataClassRight;
	std::string _description;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CPhysicalRelationshipDoc);
};

CAF_DECLARE_SMART_POINTER(CPhysicalRelationshipDoc);

}

#endif
