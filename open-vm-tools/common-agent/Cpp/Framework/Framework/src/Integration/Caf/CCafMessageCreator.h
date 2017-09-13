/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (c) 2009-2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef CCafMessageCreator_h_
#define CCafMessageCreator_h_

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CCafMessageCreator);

class INTEGRATIONCAF_LINKAGE CCafMessageCreator {
public:
	static SmartPtrIIntMessage createPayloadEnvelope(
			const SmartPtrCResponseDoc& response,
			const std::string& relFilename,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage createPayloadEnvelope(
			const SmartPtrCErrorResponseDoc& errorResponse,
			const std::string& relFilename,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage createPayloadEnvelope(
			const SmartPtrCMgmtRequestDoc& mgmtRequest,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage createPayloadEnvelope(
			const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
			const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
			const IIntMessage::SmartPtrCHeaders& newHeaders = IIntMessage::SmartPtrCHeaders(),
			const IIntMessage::SmartPtrCHeaders& origHeaders = IIntMessage::SmartPtrCHeaders());

public:
	static SmartPtrIIntMessage createFromProviderResponse(
			const SmartPtrCDynamicByteArray& providerResponse,
			const std::string& relFilename,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

public:
	static SmartPtrIIntMessage create(
			const SmartPtrCDynamicByteArray payload,
			const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

public:
	static SmartPtrIIntMessage create(
			const SmartPtrCMgmtRequestDoc& mgmtRequest,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage create(
			const SmartPtrCDiagRequestDoc& diagRequest,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage create(
			const SmartPtrCInstallRequestDoc& installRequest,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage create(
			const SmartPtrCProviderRegDoc& providerReg,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage create(
			const SmartPtrCProviderCollectSchemaRequestDoc& providerCollectSchemaRequest,
			const std::string& relFilename,
			const std::string& relDirectory,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

	static SmartPtrIIntMessage create(
			const SmartPtrCProviderRequestDoc& providerRequest,
			const std::string& relFilename,
			const std::string& relDirectory,
			const std::string& providerUri,
			const IIntMessage::SmartPtrCHeaders& headers = IIntMessage::SmartPtrCHeaders());

private:
	static SmartPtrIIntMessage createPayloadEnvelope(
			const std::string& payloadType,
			const std::string& payloadStr,
			const UUID& clientId,
			const UUID& requestId,
			const std::string& pmeId,
			const std::string& payloadVersion,
			const IIntMessage::SmartPtrCHeaders& newHeaders = IIntMessage::SmartPtrCHeaders(),
			const IIntMessage::SmartPtrCHeaders& origHeaders = IIntMessage::SmartPtrCHeaders(),
			const SmartPtrCAttachmentCollectionDoc& attachmentCollection = SmartPtrCAttachmentCollectionDoc(),
			const SmartPtrCProtocolCollectionDoc& protocolCollection = SmartPtrCProtocolCollectionDoc());

private:
	CAF_CM_DECLARE_NOCREATE(CCafMessageCreator);
};

}

#endif // #ifndef CCafMessageCreator_h_
