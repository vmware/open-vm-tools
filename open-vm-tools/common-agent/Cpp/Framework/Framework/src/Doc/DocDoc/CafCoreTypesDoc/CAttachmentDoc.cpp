/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"

using namespace Caf;

/// A simple container for objects of type Attachment
CAttachmentDoc::CAttachmentDoc() :
	_isInitialized(false),
	_isReference(false),
	_cmsPolicy(CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED) {}
CAttachmentDoc::~CAttachmentDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CAttachmentDoc::initialize(
	const std::string name,
	const std::string type,
	const std::string uri,
	const bool isReference,
	const CMS_POLICY cmsPolicy) {
	if (! _isInitialized) {
		_name = name;
		_type = type;
		_uri = uri;
		_isReference = isReference;
		_cmsPolicy = cmsPolicy;

		_isInitialized = true;
	}
}

/// Accessor for the Name
std::string CAttachmentDoc::getName() const {
	return _name;
}

/// Accessor for the Type
std::string CAttachmentDoc::getType() const {
	return _type;
}

/// Accessor for the Uri
std::string CAttachmentDoc::getUri() const {
	return _uri;
}

/// Accessor for the IsReference
bool CAttachmentDoc::getIsReference() const {
	return _isReference;
}

/// Accessor for the CMS Policy
CMS_POLICY CAttachmentDoc::getCmsPolicy() const {
	return _cmsPolicy;
}


CMS_POLICY _cmsPolicy;




