/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ProviderResultsDoc/CDefinitionObjectCollectionDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/ProviderResultsDoc/CCdifDoc.h"

using namespace Caf;

/// A simple container for objects of type Cdif
CCdifDoc::CCdifDoc() :
	_isInitialized(false) {}
CCdifDoc::~CCdifDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CCdifDoc::initialize(
	const SmartPtrCRequestIdentifierDoc requestIdentifier,
	const SmartPtrCDefinitionObjectCollectionDoc definitionObjectCollection,
	const SmartPtrCSchemaDoc schema) {
	if (! _isInitialized) {
		_requestIdentifier = requestIdentifier;
		_definitionObjectCollection = definitionObjectCollection;
		_schema = schema;

		_isInitialized = true;
	}
}

/// Accessor for the RequestIdentifier
SmartPtrCRequestIdentifierDoc CCdifDoc::getRequestIdentifier() const {
	return _requestIdentifier;
}

/// Accessor for the DefinitionObjectCollection
SmartPtrCDefinitionObjectCollectionDoc CCdifDoc::getDefinitionObjectCollection() const {
	return _definitionObjectCollection;
}

/// Accessor for the Schema
SmartPtrCSchemaDoc CCdifDoc::getSchema() const {
	return _schema;
}






