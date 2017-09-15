/*
 *  Author: bwilliams
 *  Created: July 3, 2015
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#include "stdafx.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"

using namespace Caf;

/// A simple container for objects of type PayloadEnvelope
CPayloadEnvelopeDoc::CPayloadEnvelopeDoc() :
	_clientId(CAFCOMMON_GUID_NULL),
	_requestId(CAFCOMMON_GUID_NULL),
	_isInitialized(false) {}
CPayloadEnvelopeDoc::~CPayloadEnvelopeDoc() {}

/// Initializes the object with everything required by this
/// container. Once initialized, this object cannot
/// be changed (i.e. it is immutable).
void CPayloadEnvelopeDoc::initialize(
	const UUID& clientId,
	const UUID& requestId,
	const std::string& pmeId,
	const std::string& payloadType,
	const std::string& payloadVersion,
	const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
	const SmartPtrCProtocolCollectionDoc& protocolCollection,
	const SmartPtrCPropertyCollectionDoc& headerCollection,
	const std::string version) {
	if (! _isInitialized) {
		_clientId = clientId;
		_requestId = requestId;
		_pmeId = pmeId;
		_payloadType = payloadType;
		_payloadVersion = payloadVersion;
		_attachmentCollection = attachmentCollection;
		_protocolCollection = protocolCollection;
		_headerCollection = headerCollection;
		_version = version;

		_isInitialized = true;
	}
}

/// Accessor for the ClientId
UUID CPayloadEnvelopeDoc::getClientId() const {
	return _clientId;
}

/// Accessor for the RequestId
UUID CPayloadEnvelopeDoc::getRequestId() const {
	return _requestId;
}

/// Accessor for the PmeId
std::string CPayloadEnvelopeDoc::getPmeId() const {
	return _pmeId;
}

/// Accessor for the PayloadType
std::string CPayloadEnvelopeDoc::getPayloadType() const {
	return _payloadType;
}

/// Accessor for the PayloadVersion
std::string CPayloadEnvelopeDoc::getPayloadVersion() const {
	return _payloadVersion;
}

/// Accessor for the Protocol Collection
SmartPtrCProtocolCollectionDoc CPayloadEnvelopeDoc::getProtocolCollection() const {
	return _protocolCollection;
}

/// Accessor for the AttachmentCollection
SmartPtrCAttachmentCollectionDoc CPayloadEnvelopeDoc::getAttachmentCollection() const {
	return _attachmentCollection;
}

/// Accessor for the Headers
SmartPtrCPropertyCollectionDoc CPayloadEnvelopeDoc::getHeaderCollection() const {
	return _headerCollection;
}

/// Accessor for the version
std::string CPayloadEnvelopeDoc::getVersion() const {
	return _version;
}





