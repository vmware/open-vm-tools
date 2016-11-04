/*
 *  Author: bwilliams
 *  Created: July 3, 2015
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CPayloadEnvelopeDoc_h_
#define CPayloadEnvelopeDoc_h_


#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"

namespace Caf {

/// A simple container for objects of type PayloadEnvelope
class PAYLOADENVELOPEDOC_LINKAGE CPayloadEnvelopeDoc {
public:
	CPayloadEnvelopeDoc();
	virtual ~CPayloadEnvelopeDoc();

public:
	/// Initializes the object with everything required by this
	/// container. Once initialized, this object cannot
	/// be changed (i.e. it is immutable).
	void initialize(
		const UUID& clientId,
		const UUID& requestId,
		const std::string& pmeId,
		const std::string& payloadType,
		const std::string& payloadVersion,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
		const SmartPtrCProtocolCollectionDoc& protocolCollection = SmartPtrCProtocolCollectionDoc(),
		const SmartPtrCPropertyCollectionDoc& headerCollection = SmartPtrCPropertyCollectionDoc(),
		const std::string version = "1.0");

public:
	/// Accessor for the ClientId
	UUID getClientId() const;

	/// Accessor for the RequestId
	UUID getRequestId() const;

	/// Accessor for the PmeId
	std::string getPmeId() const;

	/// Accessor for the PayloadType
	std::string getPayloadType() const;

	/// Accessor for the PayloadVersion
	std::string getPayloadVersion() const;

	/// Accessor for the Protocol Collection
	SmartPtrCProtocolCollectionDoc getProtocolCollection() const;

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const;

	/// Accessor for the Headers
	SmartPtrCPropertyCollectionDoc getHeaderCollection() const;

	/// Accessor for the version
	std::string getVersion() const;

private:
	UUID _clientId;
	UUID _requestId;
	std::string _pmeId;
	std::string _payloadType;
	std::string _payloadVersion;
	SmartPtrCAttachmentCollectionDoc _attachmentCollection;
	SmartPtrCProtocolCollectionDoc _protocolCollection;
	SmartPtrCPropertyCollectionDoc _headerCollection;
	std::string _version;
	bool _isInitialized;

private:
	CAF_CM_DECLARE_NOCOPY(CPayloadEnvelopeDoc);
};

CAF_DECLARE_SMART_POINTER(CPayloadEnvelopeDoc);

}

#endif
