/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/CafInstallRequestXml/CafInstallRequestXmlRoots.h"
#include "Doc/DocXml/DiagRequestXml/DiagRequestXmlRoots.h"
#include "Doc/DocXml/MgmtRequestXml/MgmtRequestXmlRoots.h"
#include "Doc/DocXml/ProviderInfraXml/ProviderInfraXmlRoots.h"
#include "Doc/DocXml/ProviderRequestXml/ProviderRequestXmlRoots.h"
#include "Doc/DocXml/ResponseXml/ResponseXmlRoots.h"

#include "Doc/DocUtils/EnumConvertersXml.h"
#include "Doc/DocXml/CafCoreTypesXml/AttachmentCollectionXml.h"

#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Integration/Caf/CCafMessageHeadersWriter.h"
#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CPropertyDoc.h"
#include "Doc/CafCoreTypesDoc/CProtocolCollectionDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallRequestDoc.h"
#include "Doc/DiagRequestDoc/CDiagRequestDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Doc/ProviderInfraDoc/CProviderRegDoc.h"
#include "Doc/ProviderRequestDoc/CProviderCollectSchemaRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestHeaderDoc.h"
#include "Doc/ResponseDoc/CErrorResponseDoc.h"
#include "Doc/ResponseDoc/CResponseDoc.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Xml/XmlUtils/CXmlElement.h"
#include "Integration/Caf/CCafMessagePayload.h"
#include "Integration/Caf/CCafMessageCreator.h"
#include "Doc/DocXml/PayloadEnvelopeXml/PayloadEnvelopeXmlRoots.h"

using namespace Caf;

SmartPtrIIntMessage CCafMessageCreator::createPayloadEnvelope(
		const SmartPtrCResponseDoc& response,
		const std::string& relFilename,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "createPayloadEnvelope");
	CAF_CM_VALIDATE_SMARTPTR(response);
	CAF_CM_VALIDATE_STRING(relFilename);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setRelFilename(relFilename);

	const SmartPtrIIntMessage rc = createPayloadEnvelope(
			"response",
			XmlRoots::saveResponseToString(response),
			response->getClientId(),
			response->getRequestId(),
			response->getPmeId(),
			response->getResponseHeader()->getVersion(),
			messageHeadersWriter->getHeaders(),
			headers,
			response->getAttachmentCollection());

	return rc;
}

SmartPtrIIntMessage CCafMessageCreator::createPayloadEnvelope(
		const SmartPtrCErrorResponseDoc& errorResponse,
		const std::string& relFilename,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "createPayloadEnvelope");
	CAF_CM_VALIDATE_SMARTPTR(errorResponse);
	CAF_CM_VALIDATE_STRING(relFilename);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setRelFilename(relFilename);

	const SmartPtrIIntMessage rc = createPayloadEnvelope(
			"errorResponse",
			XmlRoots::saveErrorResponseToString(errorResponse),
			errorResponse->getClientId(),
			errorResponse->getRequestId(),
			errorResponse->getPmeId(),
			errorResponse->getResponseHeader()->getVersion(),
			messageHeadersWriter->getHeaders(),
			headers);

	return rc;
}

SmartPtrIIntMessage CCafMessageCreator::createPayloadEnvelope(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "createPayloadEnvelope");
	CAF_CM_VALIDATE_SMARTPTR(mgmtRequest);

	const SmartPtrIIntMessage rc = createPayloadEnvelope(
			"mgmtRequest",
			XmlRoots::saveMgmtRequestToString(mgmtRequest),
			mgmtRequest->getClientId(),
			mgmtRequest->getRequestId(),
			mgmtRequest->getPmeId(),
			mgmtRequest->getRequestHeader()->getVersion(),
			IIntMessage::SmartPtrCHeaders(),
			headers,
			mgmtRequest->getAttachmentCollection(),
			mgmtRequest->getRequestHeader()->getProtocolCollection());

	return rc;
}

SmartPtrIIntMessage CCafMessageCreator::createPayloadEnvelope(
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope,
		const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
		const IIntMessage::SmartPtrCHeaders& newHeaders,
		const IIntMessage::SmartPtrCHeaders& origHeaders) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "createPayloadEnvelope");
	CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);

	SmartPtrCPayloadEnvelopeDoc payloadEnvelopeNew = payloadEnvelope;
	if (attachmentCollection.size() > 0) {
		SmartPtrCAttachmentCollectionDoc attachmentCollectionDoc;
		attachmentCollectionDoc.CreateInstance();
		attachmentCollectionDoc->initialize(attachmentCollection);

		payloadEnvelopeNew.CreateInstance();
		payloadEnvelopeNew->initialize(
				payloadEnvelope->getClientId(),
				payloadEnvelope->getRequestId(),
				payloadEnvelope->getPmeId(),
				payloadEnvelope->getPayloadType(),
				payloadEnvelope->getPayloadVersion(),
				attachmentCollectionDoc,
				payloadEnvelope->getProtocolCollection(),
				payloadEnvelope->getHeaderCollection(),
				payloadEnvelope->getVersion());
	}

	const std::string payloadEnvelopeStr =
			XmlRoots::savePayloadEnvelopeToString(payloadEnvelopeNew);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(payloadEnvelopeStr, newHeaders, origHeaders);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::createFromProviderResponse(
		const SmartPtrCDynamicByteArray& providerResponse,
		const std::string& relFilename,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "createFromProviderResponse");
	CAF_CM_VALIDATE_SMARTPTR(providerResponse);
	CAF_CM_VALIDATE_STRING(relFilename);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("providerResponse");
	messageHeadersWriter->setRelFilename(relFilename);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initialize(providerResponse,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCDynamicByteArray payload,
		const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(payload);

	const std::string payloadStr = reinterpret_cast<const char*>(payload->getPtr());
	const SmartPtrCXmlElement payloadXml =
			CXmlUtils::parseString(payloadStr, std::string());

	payloadXml->removeChild("attachmentCollection");
	if (! attachmentCollection.empty()) {
		SmartPtrCAttachmentCollectionDoc attachmentCollectionDoc;
		attachmentCollectionDoc.CreateInstance();
		attachmentCollectionDoc->initialize(attachmentCollection);

		const SmartPtrCXmlElement attachmentCollectionXml =
				payloadXml->createAndAddElement("attachmentCollection");
		AttachmentCollectionXml::add(attachmentCollectionDoc, attachmentCollectionXml);
	}

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(
			payloadXml->saveToString(), CIntMessage::SmartPtrCHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCMgmtRequestDoc& mgmtRequest,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(mgmtRequest);

	const std::string mgmtRequestStr =
		XmlRoots::saveMgmtRequestToString(mgmtRequest);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("mgmtRequest");

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(mgmtRequestStr,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCDiagRequestDoc& diagRequest,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(diagRequest);

	const std::string diagRequestStr =
		XmlRoots::saveDiagRequestToString(diagRequest);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("diagRequest");

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(diagRequestStr,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCInstallRequestDoc& installRequest,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(installRequest);

	const std::string installRequestStr =
		XmlRoots::saveInstallRequestToString(installRequest);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("installRequest");

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(installRequestStr,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCProviderRegDoc& providerReg,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(providerReg);

	const std::string installRequestStr =
		XmlRoots::saveProviderRegToString(providerReg);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("providerReg");

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(installRequestStr,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCProviderCollectSchemaRequestDoc& providerCollectSchemaRequest,
		const std::string& relFilename,
		const std::string& relDirectory,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(providerCollectSchemaRequest);
	CAF_CM_VALIDATE_STRING(relFilename);
	CAF_CM_VALIDATE_STRING(relDirectory);

	const std::string providerCollectSchemaRequestMem =
			XmlRoots::saveProviderCollectSchemaRequestToString(providerCollectSchemaRequest);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("providerCollectSchemaRequest");
	messageHeadersWriter->setRelFilename(relFilename);
	messageHeadersWriter->setRelDirectory(relDirectory);

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(providerCollectSchemaRequestMem,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::create(
		const SmartPtrCProviderRequestDoc& providerRequest,
		const std::string& relFilename,
		const std::string& relDirectory,
		const std::string& providerUri,
		const IIntMessage::SmartPtrCHeaders& headers) {
	CAF_CM_STATIC_FUNC_VALIDATE("CCafMessageCreator", "create");
	CAF_CM_VALIDATE_SMARTPTR(providerRequest);
	CAF_CM_VALIDATE_STRING(relFilename);
	CAF_CM_VALIDATE_STRING(relDirectory);

	SmartPtrCProviderRequestHeaderDoc requestHeader = providerRequest->getRequestHeader();
	SmartPtrCPropertyCollectionDoc propertyBag = requestHeader->getEchoPropertyBag();

	std::deque<SmartPtrCPropertyDoc> newProperties;
	if (!propertyBag.IsNull()) {
		newProperties.assign(propertyBag->getProperty().begin(), propertyBag->getProperty().end());
	}

	std::deque<std::string> relDirectoryValue(1, relDirectory);
	SmartPtrCPropertyDoc relDirectoryProp;
	relDirectoryProp.CreateInstance();
	relDirectoryProp->initialize("relDirectory", PROPERTY_STRING, relDirectoryValue);
	newProperties.push_back(relDirectoryProp);

	std::deque<std::string> providerUriValue(1, providerUri);
	SmartPtrCPropertyDoc providerUriProp;
	providerUriProp.CreateInstance();
	providerUriProp->initialize("providerUri", PROPERTY_STRING, providerUriValue);
	newProperties.push_back(providerUriProp);

	SmartPtrCPropertyCollectionDoc newPropertyBag;
	newPropertyBag.CreateInstance();
	newPropertyBag->initialize(newProperties);

	SmartPtrCProviderRequestHeaderDoc newRequestHeader;
	newRequestHeader.CreateInstance();
	newRequestHeader->initialize(requestHeader->getRequestConfig(), newPropertyBag);

	SmartPtrCProviderRequestDoc newProviderRequest;
	newProviderRequest.CreateInstance();
	newProviderRequest->initialize(providerRequest->getClientId(),
			providerRequest->getRequestId(), providerRequest->getPmeId(), newRequestHeader,
			providerRequest->getBatch(), providerRequest->getAttachmentCollection());

	const std::string providerRequestMem =
			XmlRoots::saveProviderRequestToString(newProviderRequest);

	SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
			CCafMessageHeadersWriter::create();
	messageHeadersWriter->setPayloadType("providerRequest");
	messageHeadersWriter->setRelFilename(relFilename);
	messageHeadersWriter->setRelDirectory(relDirectory);

	if (! providerUri.empty()) {
		messageHeadersWriter->setProviderUri(providerUri);
	}

	SmartPtrCIntMessage messageImpl;
	messageImpl.CreateInstance();
	messageImpl->initializeStr(providerRequestMem,
			messageHeadersWriter->getHeaders(), headers);

	return messageImpl;
}

SmartPtrIIntMessage CCafMessageCreator::createPayloadEnvelope(
		const std::string& payloadType,
		const std::string& payloadStr,
		const UUID& clientId,
		const UUID& requestId,
		const std::string& pmeId,
		const std::string& payloadVersion,
		const IIntMessage::SmartPtrCHeaders& newHeaders,
		const IIntMessage::SmartPtrCHeaders& origHeaders,
		const SmartPtrCAttachmentCollectionDoc& attachmentCollection,
		const SmartPtrCProtocolCollectionDoc& protocolCollection) {
	CAF_CM_STATIC_FUNC_LOG_VALIDATE("CCafMessageCreator", "createPayloadEnvelope");
	CAF_CM_VALIDATE_STRING(payloadType);
	CAF_CM_VALIDATE_STRING(payloadStr);
	// clientId is optional
	// requestId is optional
	// pmeId is optional
	CAF_CM_VALIDATE_STRING(payloadVersion);

	SmartPtrCIntMessage messageImpl;
	if (::IsEqualGUID(requestId, CAFCOMMON_GUID_NULL)) {
		CAF_CM_LOG_WARN_VA1("Message is not associated with a request - %s", payloadStr.c_str());
	} else {
		const std::string outputDir =
				AppConfigUtils::getRequiredString(_sAppConfigGlobalParamOutputDir);

		const std::string destAttachmentFilename =
				BasePlatform::UuidToString(requestId) + "-EnvelopePayload.xml";
		const std::string destAttachmentPath = FileSystemUtils::buildPath(
				outputDir, "att", destAttachmentFilename);
		const std::string destAttachmentUri =
				"file:///" + destAttachmentPath + "?relPath=" + destAttachmentFilename;

		const SmartPtrCDynamicByteArray payload =
				CCafMessagePayload::createBufferFromStr(payloadStr);
		FileSystemUtils::saveByteFile(destAttachmentPath, payload);

		const std::string cmsPolicyStr = AppConfigUtils::getRequiredString(
				"security", "cms_policy");
		const CMS_POLICY cmsPolicy =
				EnumConvertersXml::convertStringToCmsPolicy(cmsPolicyStr);

		SmartPtrCAttachmentDoc envelopePayloadAttachment;
		envelopePayloadAttachment.CreateInstance();
		envelopePayloadAttachment->initialize("_EnvelopePayload_", "EnvelopePayload",
				destAttachmentUri, false, cmsPolicy);

		std::deque<SmartPtrCAttachmentDoc> attachmentCollectionNew;
		if (! attachmentCollection.IsNull()) {
			attachmentCollectionNew = attachmentCollection->getAttachment();
		}
		attachmentCollectionNew.push_back(envelopePayloadAttachment);

		SmartPtrCAttachmentCollectionDoc attachmentCollectionDocNew;
		attachmentCollectionDocNew.CreateInstance();
		attachmentCollectionDocNew->initialize(attachmentCollectionNew);

		SmartPtrCPayloadEnvelopeDoc payloadEnvelope;
		payloadEnvelope.CreateInstance();
		payloadEnvelope->initialize(
				clientId,
				requestId,
				pmeId,
				payloadType,
				payloadVersion,
				attachmentCollectionDocNew,
				protocolCollection);

		const std::string payloadEnvelopeStr =
				XmlRoots::savePayloadEnvelopeToString(payloadEnvelope);

		const CIntMessage::SmartPtrCHeaders mergedHeaders =
				CIntMessage::mergeHeaders(newHeaders, origHeaders);

		SmartPtrCCafMessageHeadersWriter messageHeadersWriter =
				CCafMessageHeadersWriter::create();
		messageHeadersWriter->setPayloadType(payloadType);

		messageImpl.CreateInstance();
		messageImpl->initializeStr(
				payloadEnvelopeStr, messageHeadersWriter->getHeaders(), mergedHeaders);
	}

	return messageImpl;
}
