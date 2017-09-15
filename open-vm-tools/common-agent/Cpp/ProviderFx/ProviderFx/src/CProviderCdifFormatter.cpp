/*
 *	Author: bwilliams
 *	Created: May 3, 2012
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocXml/ProviderResultsXml/ProviderResultsXmlRoots.h"
#include "Doc/DocXml/ResponseXml/ResponseXmlRoots.h"

#include "Doc/DocUtils/EnumConvertersXml.h"

#include "Doc/CafCoreTypesDoc/CAttachmentCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/CafCoreTypesDoc/CAttachmentNameCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CInlineAttachmentDoc.h"
#include "Doc/ProviderResultsDoc/CCdifDoc.h"
#include "Doc/ProviderResultsDoc/CDefinitionObjectCollectionDoc.h"
#include "Doc/ProviderResultsDoc/CRequestIdentifierDoc.h"
#include "Doc/ProviderResultsDoc/CSchemaDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Doc/ResponseDoc/CProviderResponseDoc.h"
#include "Doc/ResponseDoc/CResponseHeaderDoc.h"
#include "Doc/SchemaTypesDoc/CActionClassDoc.h"
#include "Doc/SchemaTypesDoc/CCollectMethodDoc.h"
#include "Doc/SchemaTypesDoc/CDataClassInstanceDoc.h"
#include "Doc/SchemaTypesDoc/CMethodDoc.h"
#include "CProviderCdifFormatter.h"
#include "Doc/DocXml/ProviderResultsXml/ProviderResultsXmlLink.h"
#include "Doc/DocXml/ResponseXml/ResponseXmlLink.h"
#include "../../Framework/src/Doc/DocUtils/DocUtilsLink.h"

using namespace Caf;

CProviderCdifFormatter::CProviderCdifFormatter() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CProviderCdifFormatter") {
}

CProviderCdifFormatter::~CProviderCdifFormatter() {
}

void CProviderCdifFormatter::initialize(
	const SmartPtrCRequestIdentifierDoc requestIdentifier,
	const SmartPtrCSchemaDoc schema,
	const std::string outputFilePath) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(requestIdentifier);
	CAF_CM_VALIDATE_SMARTPTR(schema);
	CAF_CM_VALIDATE_STRING(outputFilePath);

	_requestIdentifier = requestIdentifier;
	_schema = schema;
	_outputFilePath = outputFilePath;

	_isInitialized = true;
}

void CProviderCdifFormatter::finished() {
	CAF_CM_FUNCNAME_VALIDATE("finished");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCDefinitionObjectCollectionDoc definitionObjectCollection;
	if (!_defnObjCollection.empty()) {
		definitionObjectCollection.CreateInstance();
		definitionObjectCollection->initialize(_defnObjCollection);
	}

	SmartPtrCCdifDoc cdifDoc;
	cdifDoc.CreateInstance();
	cdifDoc->initialize(_requestIdentifier, definitionObjectCollection, _schema);

	const std::string cdifXml = XmlRoots::saveCdifToString(cdifDoc);

	CAF_CM_LOG_DEBUG_VA1("Writing CDIF to file - %s", _outputFilePath.c_str());
	FileSystemUtils::saveTextFile(_outputFilePath, cdifXml);

	saveProviderResponse();
}

void CProviderCdifFormatter::addInstance(
	const SmartPtrCDataClassInstanceDoc dataClassInstance) {
	CAF_CM_FUNCNAME_VALIDATE("format");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(dataClassInstance);

	const std::string defnObjXml = DefnObjectConverter::toString(dataClassInstance);
	_defnObjCollection.push_back(defnObjXml);
}

std::string CProviderCdifFormatter::getOutputFilePath() const {
	CAF_CM_FUNCNAME_VALIDATE("getOutputFilePath");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	return _outputFilePath;
}

void CProviderCdifFormatter::addAttachment(const SmartPtrCAttachmentDoc attachment) {
	CAF_CM_FUNCNAME_VALIDATE("addAttachment");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(attachment);

	_attachmentCollectionInner.push_back(attachment);
}

void CProviderCdifFormatter::saveProviderResponse() {
	CAF_CM_FUNCNAME_VALIDATE("saveProviderResponse");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const SmartPtrCAttachmentDoc attachment = createAttachment();
	_attachmentCollectionInner.push_back(attachment);

	const SmartPtrCAttachmentCollectionDoc attachmentCollection =
		createAttachmentCollection();

	const SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection =
		createAttachmentNameCollection();

	std::string classNamespace;
	std::string className;
	std::string classVersion;
	std::string operationName;
	const SmartPtrCActionClassDoc actionClass = _requestIdentifier->getActionClass();
	if (!actionClass.IsNull()) {
		classNamespace = actionClass->getNamespaceVal();
		className = actionClass->getName();
		classVersion = actionClass->getVersion();

		const SmartPtrCCollectMethodDoc collectMethod = actionClass->getCollectMethod();
		const std::deque<SmartPtrCMethodDoc> methodCollection =
			actionClass->getMethodCollection();
		if (!collectMethod.IsNull()) {
			operationName = collectMethod->getName();
		} else if (!methodCollection.empty()) {
			const SmartPtrCMethodDoc method = methodCollection.front();
			operationName = method->getName();
		}
	}

	SmartPtrCManifestDoc manifest;
	manifest.CreateInstance();
	manifest->initialize(classNamespace, className, classVersion,
		_requestIdentifier->getJobId(), operationName, attachmentNameCollection);

	SmartPtrCResponseHeaderDoc responseHeader;
	responseHeader.CreateInstance();
	responseHeader->initialize(
			"1.0",
			CDateTimeUtils::getCurrentDateTime(),
			0,
			true,
			_requestIdentifier->getSessionId());

	SmartPtrCProviderResponseDoc providerResponse;
	providerResponse.CreateInstance();
	providerResponse->initialize(_requestIdentifier->getClientId(),
		_requestIdentifier->getRequestId(), _requestIdentifier->getPmeId(),
		responseHeader, manifest, attachmentCollection,
		SmartPtrCStatisticsDoc());

	const std::string attachmentDirPath = FileSystemUtils::getDirname(_outputFilePath);
	const std::string providerResponseStr = XmlRoots::saveProviderResponseToString(providerResponse);
	const std::string providerResponsePath = FileSystemUtils::buildPath(
		attachmentDirPath, _sProviderResponseFilename);
	FileSystemUtils::saveTextFile(providerResponsePath, providerResponseStr);
	CAF_CM_LOG_DEBUG_VA1("Saved provider response file - %s", providerResponsePath.c_str());
}

SmartPtrCAttachmentDoc CProviderCdifFormatter::createAttachment() const {
	CAF_CM_FUNCNAME_VALIDATE("createAttachment");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	const std::string attachmentFileName = FileSystemUtils::getBasename(_outputFilePath);
	const std::string jobIdStr = BasePlatform::UuidToString(_requestIdentifier->getJobId());
	const std::string attachmentName = jobIdStr + "." + attachmentFileName;
	const std::string cdifAttachmentFilePathTmp =
		FileSystemUtils::normalizePathWithForward(_outputFilePath);

	const std::string cmsPolicyStr = AppConfigUtils::getRequiredString(
			"security", "cms_policy");;

	SmartPtrCAttachmentDoc attachment;
	attachment.CreateInstance();
	attachment->initialize(attachmentName, "cdif",
		"file:///" + cdifAttachmentFilePathTmp, false,
		EnumConvertersXml::convertStringToCmsPolicy(cmsPolicyStr));

	return attachment;
}

SmartPtrCAttachmentCollectionDoc CProviderCdifFormatter::createAttachmentCollection() const {
	CAF_CM_FUNCNAME_VALIDATE("createAttachmentCollection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	SmartPtrCAttachmentCollectionDoc attachmentCollection;
	attachmentCollection.CreateInstance();
	attachmentCollection->initialize(_attachmentCollectionInner,
		std::deque<SmartPtrCInlineAttachmentDoc>());

	return attachmentCollection;
}

SmartPtrCAttachmentNameCollectionDoc CProviderCdifFormatter::createAttachmentNameCollection() const {
	CAF_CM_FUNCNAME_VALIDATE("createAttachmentNameCollection");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

	std::deque<std::string> attachmentNameCollectionInner;
	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(
		_attachmentCollectionInner); attachmentIter; attachmentIter++) {
		const SmartPtrCAttachmentDoc attachment = *attachmentIter;

		attachmentNameCollectionInner.push_back(attachment->getName());
	}

	SmartPtrCAttachmentNameCollectionDoc attachmentNameCollection;
	attachmentNameCollection.CreateInstance();
	attachmentNameCollection->initialize(attachmentNameCollectionInner);

	return attachmentNameCollection;
}
