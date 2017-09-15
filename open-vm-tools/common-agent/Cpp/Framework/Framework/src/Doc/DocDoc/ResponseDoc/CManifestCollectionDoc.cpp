/*
 *  Author: bwilliams
 *  Created: April 6, 2012
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 *
 */

#include "stdafx.h"

#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CManifestCollectionDoc.h"

using namespace Caf;

/// A simple container for objects of type ManifestCollection
CManifestCollectionDoc::CManifestCollectionDoc() :
	_isInitialized(false) {}
CManifestCollectionDoc::~CManifestCollectionDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CManifestCollectionDoc::initialize(
	const std::deque<SmartPtrCManifestDoc> manifest) {
	if (! _isInitialized) {
		_manifest = manifest;

		_isInitialized = true;
	}
}

/// Accessor for the Manifest
std::deque<SmartPtrCManifestDoc> CManifestCollectionDoc::getManifest() const {
	return _manifest;
}






