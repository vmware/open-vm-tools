/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CCollectMethodDoc_h_
#define CCollectMethodDoc_h_


#include "Doc/SchemaTypesDoc/CClassIdentifierDoc.h"
#include "Doc/SchemaTypesDoc/CInstanceParameterDoc.h"
#include "Doc/SchemaTypesDoc/CMethodParameterDoc.h"

namespace Caf {

/// Definition of a collection method on a class
class SCHEMATYPESDOC_LINKAGE CCollectMethodDoc {
public:
	CCollectMethodDoc();
	virtual ~CCollectMethodDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const std::string name,
		const std::deque<SmartPtrCMethodParameterDoc> parameterCollection = std::deque<SmartPtrCMethodParameterDoc>(),
		const std::deque<SmartPtrCInstanceParameterDoc> instanceParameterCollection = std::deque<SmartPtrCInstanceParameterDoc>(),
		const std::deque<SmartPtrCClassIdentifierDoc> returnValCollection = std::deque<SmartPtrCClassIdentifierDoc>(),
		const std::deque<SmartPtrCClassIdentifierDoc> eventValCollection = std::deque<SmartPtrCClassIdentifierDoc>(),
		const std::deque<SmartPtrCClassIdentifierDoc> errorCollection = std::deque<SmartPtrCClassIdentifierDoc>());

public:
	/// name of the collection method
	std::string getName() const;

	/// Definition of a parameter that passes simple types to the collection method
	std::deque<SmartPtrCMethodParameterDoc> getParameterCollection() const;

	/// Definition of a parameter passing data class instances to the collection method
	std::deque<SmartPtrCInstanceParameterDoc> getInstanceParameterCollection() const;

	/// Accessor for the ReturnVal
	std::deque<SmartPtrCClassIdentifierDoc> getReturnValCollection() const;

	/// Accessor for the EventVal
	std::deque<SmartPtrCClassIdentifierDoc> getEventValCollection() const;

	/// A class that may be returned to indicate an error occurred during the processing of the collection method
	std::deque<SmartPtrCClassIdentifierDoc> getErrorCollection() const;

private:
	bool _isInitialized;

	std::string _name;
	std::deque<SmartPtrCMethodParameterDoc> _parameterCollection;
	std::deque<SmartPtrCInstanceParameterDoc> _instanceParameterCollection;
	std::deque<SmartPtrCClassIdentifierDoc> _returnValCollection;
	std::deque<SmartPtrCClassIdentifierDoc> _eventValCollection;
	std::deque<SmartPtrCClassIdentifierDoc> _errorCollection;

private:
	CAF_CM_DECLARE_NOCOPY(CCollectMethodDoc);
};

CAF_DECLARE_SMART_POINTER(CCollectMethodDoc);

}

#endif
