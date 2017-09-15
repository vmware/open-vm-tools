/*
 *	 Author: bwilliams
 *  Created: July 3, 2015
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Doc/DocUtils/EnumConvertersXml.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"
#include "Doc/CafCoreTypesDoc/CafCoreTypesDocTypes.h"

#include "CCmsMessage.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "CCmsMessageAttachments.h"
#include "Exception/CCafException.h"

using namespace Caf;

CCmsMessageAttachments::CCmsMessageAttachments() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CCmsMessageAttachments") {
}

CCmsMessageAttachments::~CCmsMessageAttachments() {
}

void CCmsMessageAttachments::initialize(
		const SmartPtrCCmsMessage& cmsMessage) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_SMARTPTR(cmsMessage);

	_cmsMessage = cmsMessage;

	_isInitialized = true;
}

std::deque<SmartPtrCAttachmentDoc> CCmsMessageAttachments::encryptAndSignAttachments(
		const std::deque<SmartPtrCAttachmentDoc>& sourceAttachmentCollection) const {
	CAF_CM_FUNCNAME("encryptAndSignAttachments");
	// sourceAttachmentCollection is optional

	std::deque<SmartPtrCAttachmentDoc> rc;
	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > sourceAttachmentIter(sourceAttachmentCollection);
		sourceAttachmentIter; sourceAttachmentIter++) {
		const SmartPtrCAttachmentDoc sourceAttachment = *sourceAttachmentIter;

		UriUtils::SUriRecord sourceUriRecord;
		UriUtils::parseUriString(sourceAttachment->getUri(), sourceUriRecord);

		if ((sourceUriRecord.protocol.compare("file") == 0)
				&& !sourceAttachment->getIsReference()) {
			UriUtils::SFileUriRecord sourceFileUriRecord;
			UriUtils::parseFileAddress(sourceUriRecord.address, sourceFileUriRecord);
			const std::string sourceAttachmentPath = sourceFileUriRecord.path;

			if (!FileSystemUtils::doesFileExist(sourceAttachmentPath)) {
				CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
					"File not found - %s", sourceAttachmentPath.c_str());
			}

			const SmartPtrCAttachmentDoc destAttachment = encryptAndSignAttachment(
					sourceAttachmentPath, sourceAttachment, sourceUriRecord.parameters);

			rc.push_back(destAttachment);
		} else {
			rc.push_back(sourceAttachment);
		}
	}

	return rc;
}

void CCmsMessageAttachments::enforceSecurityOnAttachments(
		const std::deque<SmartPtrCAttachmentDoc>& attachmentCollection,
		const bool isSigningEnforced,
		const bool isEncryptionEnforced) const {
	CAF_CM_FUNCNAME("enforceSecurityOnAttachments");

	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollection);
			attachmentIter; attachmentIter++) {
		const SmartPtrCAttachmentDoc attachment = *attachmentIter;

		const CMS_POLICY cmsPolicy = attachment->getCmsPolicy();
		switch(cmsPolicy) {
			case CMS_POLICY_NONE:
				enforceSigning(isSigningEnforced, attachment);
				enforceEncryption(isEncryptionEnforced, attachment);
			break;
			case CMS_POLICY_CAF_ENCRYPTED:
				enforceSigning(isSigningEnforced, attachment);
			break;
			case CMS_POLICY_CAF_SIGNED:
				enforceEncryption(isEncryptionEnforced, attachment);
			break;
			case CMS_POLICY_APP_ENCRYPTED:
				enforceSigning(isSigningEnforced, attachment);
			break;
			case CMS_POLICY_APP_SIGNED:
				enforceEncryption(isEncryptionEnforced, attachment);
			break;
			case CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED:
			case CMS_POLICY_APP_ENCRYPTED_AND_SIGNED:
			break;
			default:
				CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
					"Unknown CMS Policy - %d", static_cast<int32>(cmsPolicy));
		}
	}
}

SmartPtrCAttachmentDoc CCmsMessageAttachments::encryptAndSignAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters) const {
	CAF_CM_FUNCNAME("encryptAndSignAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	SmartPtrCAttachmentDoc destAttachment = sourceAttachment;

	std::string destAttachmentPath;
	std::string sourceAttachmentPathTmp = sourceAttachmentPath;
	SmartPtrCAttachmentDoc sourceAttachmentTmp = sourceAttachment;

	const CMS_POLICY cmsPolicy = sourceAttachmentTmp->getCmsPolicy();
	switch(cmsPolicy) {
		case CMS_POLICY_CAF_ENCRYPTED:
			encryptAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_CAF_SIGNED:
			signAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED:
			encryptAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);

			sourceAttachmentPathTmp = destAttachmentPath;
			sourceAttachmentTmp = destAttachment;

			signAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_NONE:
		case CMS_POLICY_APP_ENCRYPTED:
		case CMS_POLICY_APP_SIGNED:
		case CMS_POLICY_APP_ENCRYPTED_AND_SIGNED:
			CAF_CM_LOG_DEBUG_VA1("Passthrough CMS Policy - %s",
					EnumConvertersXml::convertCmsPolicyToString(cmsPolicy).c_str())
		break;
		default:
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown CMS Policy - %d", static_cast<int32>(cmsPolicy));
	}

	return destAttachment;
}

std::deque<SmartPtrCAttachmentDoc> CCmsMessageAttachments::decryptAndVerifyAttachments(
	const std::deque<SmartPtrCAttachmentDoc>& sourceAttachmentCollection) const {
	CAF_CM_FUNCNAME("decryptAndVerifyAttachments");
	// sourceAttachmentCollection is optional

	std::deque<SmartPtrCAttachmentDoc> rc;
	for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > sourceAttachmentIter(sourceAttachmentCollection);
		sourceAttachmentIter; sourceAttachmentIter++) {
		const SmartPtrCAttachmentDoc sourceAttachment = *sourceAttachmentIter;

		UriUtils::SUriRecord sourceUriRecord;
		UriUtils::parseUriString(sourceAttachment->getUri(), sourceUriRecord);

		if ((sourceUriRecord.protocol.compare("file") == 0)
				&& !sourceAttachment->getIsReference()) {
			UriUtils::SFileUriRecord sourceFileUriRecord;
			UriUtils::parseFileAddress(sourceUriRecord.address, sourceFileUriRecord);
			const std::string sourceAttachmentPath = sourceFileUriRecord.path;

			if (!FileSystemUtils::doesFileExist(sourceAttachmentPath)) {
				CAF_CM_EXCEPTION_VA1(ERROR_FILE_NOT_FOUND,
					"File not found - %s", sourceAttachmentPath.c_str());
			}

			const SmartPtrCAttachmentDoc destAttachment = decryptAndVerifyAttachment(
					sourceAttachmentPath, sourceAttachment, sourceUriRecord.parameters);

			rc.push_back(destAttachment);
		} else {
			rc.push_back(sourceAttachment);
		}
	}

	return rc;
}

SmartPtrCAttachmentDoc CCmsMessageAttachments::decryptAndVerifyAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters) const {
	CAF_CM_FUNCNAME("decryptAndVerifyAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	SmartPtrCAttachmentDoc destAttachment = sourceAttachment;

	std::string destAttachmentPath;
	std::string sourceAttachmentPathTmp = sourceAttachmentPath;
	SmartPtrCAttachmentDoc sourceAttachmentTmp = sourceAttachment;

	const CMS_POLICY cmsPolicy = sourceAttachmentTmp->getCmsPolicy();
	switch(cmsPolicy) {
		case CMS_POLICY_CAF_ENCRYPTED:
			decryptAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_CAF_SIGNED:
			verifyAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_CAF_ENCRYPTED_AND_SIGNED:
			verifyAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);

			sourceAttachmentPathTmp = destAttachmentPath;
			sourceAttachmentTmp = destAttachment;

			decryptAttachment(sourceAttachmentPathTmp, sourceAttachmentTmp,
					uriParameters, destAttachmentPath, destAttachment);
		break;
		case CMS_POLICY_NONE:
		case CMS_POLICY_APP_ENCRYPTED:
		case CMS_POLICY_APP_SIGNED:
		case CMS_POLICY_APP_ENCRYPTED_AND_SIGNED:
			CAF_CM_LOG_DEBUG_VA1("Passthrough CMS Policy - %s",
					EnumConvertersXml::convertCmsPolicyToString(cmsPolicy).c_str())
		break;
		default:
			CAF_CM_EXCEPTIONEX_VA1(InvalidArgumentException, E_INVALIDARG,
				"Unknown CMS Policy - %d", static_cast<int32>(cmsPolicy));
	}

	return destAttachment;
}

void CCmsMessageAttachments::signAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters,
		std::string& destAttachmentPath,
		SmartPtrCAttachmentDoc& destAttachment) const {
	CAF_CM_FUNCNAME_VALIDATE("signAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	destAttachmentPath = sourceAttachmentPath + "_signed";
	_cmsMessage->signFileToFile(sourceAttachmentPath, destAttachmentPath);

	const std::string attachmentUri = UriUtils::appendParameters(
			destAttachmentPath, uriParameters);

	destAttachment.CreateInstance();
	destAttachment->initialize(sourceAttachment->getName(), sourceAttachment->getType(),
			"file:///" + attachmentUri, sourceAttachment->getIsReference(),
			sourceAttachment->getCmsPolicy());
}

void CCmsMessageAttachments::verifyAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters,
		std::string& destAttachmentPath,
		SmartPtrCAttachmentDoc& destAttachment) const {
	CAF_CM_FUNCNAME_VALIDATE("verifyAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	destAttachmentPath = sourceAttachmentPath + "_verified";
	removeStr(destAttachmentPath, "_signed");

	_cmsMessage->verifyFileToFile(sourceAttachmentPath, destAttachmentPath);

	const std::string attachmentUri = UriUtils::appendParameters(
			destAttachmentPath, uriParameters);

	destAttachment.CreateInstance();
	destAttachment->initialize(sourceAttachment->getName(), sourceAttachment->getType(),
			"file:///" + attachmentUri, sourceAttachment->getIsReference(),
			sourceAttachment->getCmsPolicy());
}

void CCmsMessageAttachments::encryptAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters,
		std::string& destAttachmentPath,
		SmartPtrCAttachmentDoc& destAttachment) const {
	CAF_CM_FUNCNAME_VALIDATE("encryptAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	destAttachmentPath = sourceAttachmentPath + "_encrypted";
	_cmsMessage->encryptFileToFile(sourceAttachmentPath, destAttachmentPath);

	const std::string attachmentUri = UriUtils::appendParameters(
			destAttachmentPath, uriParameters);

	destAttachment.CreateInstance();
	destAttachment->initialize(sourceAttachment->getName(), sourceAttachment->getType(),
			"file:///" + attachmentUri, sourceAttachment->getIsReference(),
			sourceAttachment->getCmsPolicy());
}

void CCmsMessageAttachments::decryptAttachment(
		const std::string& sourceAttachmentPath,
		const SmartPtrCAttachmentDoc& sourceAttachment,
		const std::map<std::string, std::string>& uriParameters,
		std::string& destAttachmentPath,
		SmartPtrCAttachmentDoc& destAttachment) const {
	CAF_CM_FUNCNAME_VALIDATE("decryptAttachment");
	CAF_CM_VALIDATE_STRING(sourceAttachmentPath);
	CAF_CM_VALIDATE_SMARTPTR(sourceAttachment);

	destAttachmentPath = sourceAttachmentPath + "_decrypted";
	removeStr(destAttachmentPath, "_encrypted");

	_cmsMessage->decryptFileToFile(sourceAttachmentPath, destAttachmentPath);

	const std::string attachmentUri = UriUtils::appendParameters(
			destAttachmentPath, uriParameters);

	destAttachment.CreateInstance();
	destAttachment->initialize(sourceAttachment->getName(), sourceAttachment->getType(),
			"file:///" + attachmentUri, sourceAttachment->getIsReference(),
			sourceAttachment->getCmsPolicy());
}

void CCmsMessageAttachments::removeStr(
		std::string& sourceStr,
		const std::string& strToRemove) const {
	CAF_CM_FUNCNAME_VALIDATE("removeStr");
	CAF_CM_VALIDATE_STRING(sourceStr);
	CAF_CM_VALIDATE_STRING(strToRemove);

	const size_t index = sourceStr.find(strToRemove);
	if (index != std::string::npos) {
		sourceStr.erase(index, strToRemove.length());
	}
}

void CCmsMessageAttachments::enforceSigning(
		const bool isSigningEnforced,
		const SmartPtrCAttachmentDoc& attachment) const {
	CAF_CM_FUNCNAME("enforceSigning");
	CAF_CM_VALIDATE_SMARTPTR(attachment);

	if (isSigningEnforced) {
		CAF_CM_EXCEPTION_VA2(E_FAIL,
				"Attachment must be signed - name: %s, uri: %s",
				attachment->getName().c_str(), attachment->getUri().c_str());
	}
}

void CCmsMessageAttachments::enforceEncryption(
		const bool isEncryptionEnforced,
		const SmartPtrCAttachmentDoc& attachment) const {
	CAF_CM_FUNCNAME("enforceEncryption");
	CAF_CM_VALIDATE_SMARTPTR(attachment);

	if (isEncryptionEnforced) {
		CAF_CM_EXCEPTION_VA2(E_FAIL,
				"Attachment must be encrypted - name: %s, uri: %s",
				attachment->getName().c_str(), attachment->getUri().c_str());
	}
}
