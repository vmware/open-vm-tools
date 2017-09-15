/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CAttachmentDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "CAttachmentRequestTransformerInstance.h"
#include "Exception/CCafException.h"
#include "Integration/Caf/CCafMessagePayloadParser.h"
#include "Integration/Caf/CCafMessageCreator.h"

using namespace Caf;

CAttachmentRequestTransformerInstance::CAttachmentRequestTransformerInstance() :
	_isInitialized(false),
	CAF_CM_INIT_LOG("CAttachmentRequestTransformerInstance") {
}

CAttachmentRequestTransformerInstance::~CAttachmentRequestTransformerInstance() {
}

void CAttachmentRequestTransformerInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CAttachmentRequestTransformerInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CAttachmentRequestTransformerInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME_VALIDATE("wire");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_INTERFACE(appContext);
		CAF_CM_VALIDATE_INTERFACE(channelResolver);
	}
	CAF_CM_EXIT;
}

SmartPtrIIntMessage CAttachmentRequestTransformerInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		newMessage = message;

		const SmartPtrCPayloadEnvelopeDoc payloadEnvelope =
				CCafMessagePayloadParser::getPayloadEnvelope(message->getPayload());

		// The standard is for an optional attachment collection at the root of all
		// documents.
		const std::deque<SmartPtrCAttachmentDoc> attachmentCollection =
				payloadEnvelope->getAttachmentCollection()->getAttachment();

		if (! attachmentCollection.empty()) {
			const std::string outputDirPath = calcOutputDirPath(payloadEnvelope);

			std::deque<SmartPtrCAttachmentDoc> newAttachmentCollection;
			for (TConstIterator<std::deque<SmartPtrCAttachmentDoc> > attachmentIter(attachmentCollection);
				attachmentIter; attachmentIter++) {
				const SmartPtrCAttachmentDoc attachment = *attachmentIter;
				const std::string attachmentName = attachment->getName();
				const std::string attachmentUri = attachment->getUri();

				UriUtils::SUriRecord uriRecord;
				UriUtils::parseUriString(attachmentUri, uriRecord);

				SmartPtrCAttachmentDoc newAttachment = attachment;
				if(uriRecord.protocol.compare("file") == 0) {
					const std::string origFilePath = calcFilePath(uriRecord);
					const std::string relPath = calcRelPath(origFilePath, uriRecord);
					const std::string newFilePath = FileSystemUtils::buildPath(
						outputDirPath, relPath);

					if (origFilePath.compare(newFilePath) == 0) {
						CAF_CM_LOG_DEBUG_VA1("File path unchanged... no-op - %s", newFilePath.c_str());
					} else {
						moveFile(origFilePath, newFilePath);

						const std::string newUri = "file:///" + newFilePath + "?relPath=" + relPath;

						newAttachment.CreateInstance();
						newAttachment->initialize(attachment->getName(),
								attachment->getType(), newUri, false,
								attachment->getCmsPolicy());
					}
				}

				newAttachmentCollection.push_back(newAttachment);
			}

			newMessage = CCafMessageCreator::createPayloadEnvelope(
					payloadEnvelope, newAttachmentCollection, message->getHeaders());
		}
	}
	CAF_CM_EXIT;

	return newMessage;
}

std::string CAttachmentRequestTransformerInstance::calcOutputDirPath(
		const SmartPtrCPayloadEnvelopeDoc& payloadEnvelope) const {
	CAF_CM_FUNCNAME_VALIDATE("calcOutputDirPath");

	std::string outputDirPath;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(payloadEnvelope);

		const std::string clientIdStr =
				BasePlatform::UuidToString(payloadEnvelope->getClientId());
		const std::string requestIdStr =
				BasePlatform::UuidToString(payloadEnvelope->getRequestId());
		const std::string pmeIdStr = payloadEnvelope->getPmeId();

		const std::string outputDir = AppConfigUtils::getRequiredString(_sConfigOutputDir);

		outputDirPath = FileSystemUtils::buildPath(
			outputDir, "att", clientIdStr, requestIdStr, pmeIdStr);

		outputDirPath = CStringUtils::expandEnv(outputDirPath);
		if (! FileSystemUtils::doesDirectoryExist(outputDirPath)) {
			CAF_CM_LOG_DEBUG_VA1("Creating output directory - %s", outputDirPath.c_str());
			FileSystemUtils::createDirectory(outputDirPath);
		}
	}
	CAF_CM_EXIT;

	return outputDirPath;
}

std::string CAttachmentRequestTransformerInstance::calcFilePath(
	const UriUtils::SUriRecord& uriRecord) const {
	CAF_CM_FUNCNAME("calcFilePath");

	std::string filePath;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		UriUtils::SFileUriRecord fileUriRecord;
		UriUtils::parseFileAddress(uriRecord.address, fileUriRecord);

		filePath = CStringUtils::expandEnv(fileUriRecord.path);
		if(! FileSystemUtils::doesFileExist(filePath)) {
			CAF_CM_EXCEPTIONEX_VA1(FileNotFoundException, ERROR_FILE_NOT_FOUND,
				"URI file not found - %s", filePath.c_str());
		}
	}
	CAF_CM_EXIT;

	return filePath;
}

std::string CAttachmentRequestTransformerInstance::calcRelPath(
	const std::string& filePath,
	const UriUtils::SUriRecord& uriRecord) const {
	CAF_CM_FUNCNAME_VALIDATE("calcRelPath");

	std::string relPath;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);

		const std::map<std::string, std::string>::const_iterator iterParam =
			uriRecord.parameters.find("relPath");
		if (iterParam != uriRecord.parameters.end()) {
			relPath = iterParam->second;
		} else {
			CAF_CM_LOG_DEBUG_VA1("Attachment URI does not contain relPath - %s", uriRecord.address.c_str());
	        CAF_CM_VALIDATE_STRING(filePath);
	        relPath = FileSystemUtils::getBasename(filePath);
		}
	}
	CAF_CM_EXIT;

	return relPath;
}

void CAttachmentRequestTransformerInstance::moveFile(
	const std::string& srcFilePath,
	const std::string& dstFilePath) const {
	CAF_CM_FUNCNAME_VALIDATE("moveFile");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(srcFilePath);
		CAF_CM_VALIDATE_STRING(dstFilePath);

		if (FileSystemUtils::doesFileExist(dstFilePath)) {
			CAF_CM_LOG_WARN_VA1("File already exists - %s", dstFilePath.c_str());
		} else {
			const std::string newDirPath = FileSystemUtils::getDirname(dstFilePath);
			if (! FileSystemUtils::doesDirectoryExist(newDirPath)) {
				CAF_CM_LOG_DEBUG_VA1("Creating input directory - %s", newDirPath.c_str());
				FileSystemUtils::createDirectory(newDirPath);
			}

			CAF_CM_LOG_DEBUG_VA2("Moving file - \"%s\" to \"%s\"",
				srcFilePath.c_str(), dstFilePath.c_str());

			FileSystemUtils::moveFile(srcFilePath, dstFilePath);
		}
	}
	CAF_CM_EXIT;
}
