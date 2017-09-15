/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#ifndef CCdifDoc_h_
#define CCdifDoc_h_


#include "Doc/ProviderResultsDoc/CDefinitionObjectCollectionDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"

namespace Caf {

/// A simple container for objects of type Cdif
class PROVIDERRESULTSDOC_LINKAGE CCdifDoc {
public:
	CCdifDoc();
	virtual ~CCdifDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const SmartPtrCRequestIdentifierDoc requestIdentifier,
		const SmartPtrCDefinitionObjectCollectionDoc definitionObjectCollection,
		const SmartPtrCSchemaDoc schema);

public:
	/// Accessor for the RequestIdentifier
	SmartPtrCRequestIdentifierDoc getRequestIdentifier() const;

	/// Accessor for the DefinitionObjectCollection
	SmartPtrCDefinitionObjectCollectionDoc getDefinitionObjectCollection() const;

	/// Accessor for the Schema
	SmartPtrCSchemaDoc getSchema() const;

private:
	bool _isInitialized;

	SmartPtrCRequestIdentifierDoc _requestIdentifier;
	SmartPtrCDefinitionObjectCollectionDoc _definitionObjectCollection;
	SmartPtrCSchemaDoc _schema;

private:
	CAF_CM_DECLARE_NOCOPY(CCdifDoc);
};

CAF_DECLARE_SMART_POINTER(CCdifDoc);

}

#endif
