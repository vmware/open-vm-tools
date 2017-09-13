/*
 *  Author: bwilliams
 *  Created: July 3, 2015
 *
 *  Copyright (c) 2012 Vmware, Inc.  All rights reserved.
 *  -- VMware Confidential
 *
 */

#ifndef CPayloadEnvelopeDoc_h_
#define CPayloadEnvelopeDoc_h_

namespace Caf {

/// A simple container for objects of type PayloadEnvelope
class CPayloadEnvelopeDoc {
public:
	CPayloadEnvelopeDoc() :
		_clientId(CAFCOMMON_GUID_NULL),
		_requestId(CAFCOMMON_GUID_NULL),
		_isInitialized(false) {}
	virtual ~CPayloadEnvelopeDoc() {}

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
		const std::string version = "1.0") {
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

public:
	/// Accessor for the ClientId
	UUID getClientId() const {
		return _clientId;
	}

	/// Accessor for the RequestId
	UUID getRequestId() const {
		return _requestId;
	}

	/// Accessor for the PmeId
	std::string getPmeId() const {
		return _pmeId;
	}

	/// Accessor for the PayloadType
	std::string getPayloadType() const {
		return _payloadType;
	}

	/// Accessor for the PayloadVersion
	std::string getPayloadVersion() const {
		return _payloadVersion;
	}

	/// Accessor for the Protocol Collection
	SmartPtrCProtocolCollectionDoc getProtocolCollection() const {
		return _protocolCollection;
	}

	/// Accessor for the AttachmentCollection
	SmartPtrCAttachmentCollectionDoc getAttachmentCollection() const {
		return _attachmentCollection;
	}

	/// Accessor for the Headers
	SmartPtrCPropertyCollectionDoc getHeaderCollection() const {
		return _headerCollection;
	}

	/// Accessor for the version
	std::string getVersion() const {
		return _version;
	}

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
