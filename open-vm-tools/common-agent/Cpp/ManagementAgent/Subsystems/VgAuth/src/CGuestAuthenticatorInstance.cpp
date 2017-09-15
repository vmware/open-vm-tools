/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/Caf/CCafMessagePayload.h"
#include "CVgAuthImpersonation.h"
#include "CVgAuthInitializer.h"
#include "CVgAuthContext.h"
#include "CVgAuthUserHandle.h"
#include "Common/IAppContext.h"
#include "Doc/CafCoreTypesDoc/CAuthnAuthzCollectionDoc.h"
#include "Doc/CafCoreTypesDoc/CAuthnAuthzDoc.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "ICafObject.h"
#include "IVgAuthImpersonation.h"
#include "Integration/Core/CIntMessage.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Integration/Core/CIntMessageHeaders.h"
#include "CGuestAuthenticatorInstance.h"

using namespace Caf;

CGuestAuthenticatorInstance::CGuestAuthenticatorInstance() :
	_isInitialized(false),
	_beginImpersonation(false),
	_endImpersonation(false),
	CAF_CM_INIT_LOG("CGuestAuthenticatorInstance") {
}

CGuestAuthenticatorInstance::~CGuestAuthenticatorInstance() {
}

void CGuestAuthenticatorInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {

	CAF_CM_FUNCNAME_VALIDATE("initialize");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STL_EMPTY(ctorArgs);
		CAF_CM_VALIDATE_STL(properties);
		CAF_CM_VALIDATE_INTERFACE(configSection);

		_id = configSection->findRequiredAttribute("id");

		const std::string beginImpersonationStr =
				findOptionalProperty("beginImpersonation", properties);
		const std::string endImpersonationStr =
				findOptionalProperty("endImpersonation", properties);

		_beginImpersonation =
			(beginImpersonationStr.empty() || beginImpersonationStr.compare("false") == 0) ? false : true;
		_endImpersonation =
			(endImpersonationStr.empty() || endImpersonationStr.compare("false") == 0) ? false : true;

		_isInitialized = true;
	}
	CAF_CM_EXIT;
}

std::string CGuestAuthenticatorInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		rc = _id;
	}
	CAF_CM_EXIT;

	return rc;
}

void CGuestAuthenticatorInstance::wire(
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

SmartPtrIIntMessage CGuestAuthenticatorInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");

	SmartPtrIIntMessage newMessage;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(message);

		CAF_CM_LOG_DEBUG_VA1("Called - id: \"%s\"", _id.c_str());

		CIntMessageHeaders messageHeaders;

		const std::string vgAuthImpersonationStr = "vgAuthImpersonation";
		const SmartPtrICafObject cafObject =
			message->findOptionalObjectHeader(vgAuthImpersonationStr);
		if (cafObject.IsNull()) {
			if (_endImpersonation) {
				CAF_CM_LOG_ERROR_VA1("Cannot end impersonation without proper header - %s",
					vgAuthImpersonationStr.c_str());
			} else {
				SmartPtrCVgAuthInitializer vgAuthInitializer;
				vgAuthInitializer.CreateInstance();
				vgAuthInitializer->initialize("CAF");
				const SmartPtrCVgAuthContext vgAuthContext =
						vgAuthInitializer->getContext();

				const std::string signedSamlToken =
						getSignedSamlToken(message->getPayload());

				SmartPtrCVgAuthUserHandle vgAuthUserHandle;
				vgAuthUserHandle.CreateInstance();
				vgAuthUserHandle->initialize(
					vgAuthContext, signedSamlToken);

				if (_beginImpersonation) {
					logUserInfo("Before beginning impersonation");
					CVgAuthImpersonation::beginImpersonation(vgAuthContext, vgAuthUserHandle);
					const std::string userName = vgAuthUserHandle->getUserName(vgAuthContext);
					logUserInfo("After beginning impersonation");

					messageHeaders.insertObject(vgAuthImpersonationStr, vgAuthInitializer);
					messageHeaders.insertString("AUTHORITY", "IS_AUTHENTICATED_FULLY");
					messageHeaders.insertString("AUTHORITY_USERNAME", userName);
				}
			}
		} else {
			if (_endImpersonation) {
				SmartPtrIVgAuthImpersonation vgAuthImpersonation;
				vgAuthImpersonation.QueryInterface(cafObject, true);
				CAF_CM_VALIDATE_SMARTPTR(vgAuthImpersonation);

				logUserInfo("Before ending impersonation");
				vgAuthImpersonation->endImpersonation();
				logUserInfo("After ending impersonation");

				messageHeaders.insertString("AUTHORITY", "IS_AUTHENTICATED_ANONYMOUSLY");
				messageHeaders.insertString("AUTHORITY_USERNAME", "ANONYMOUS");
			} else {
				CAF_CM_LOG_WARN_VA1("Headers contain impersonation interface, but the ending of impersonation was not requested - %s",
					vgAuthImpersonationStr.c_str());
			}

			//messageHeaders.insertObject(vgAuthImpersonationStr, SmartPtrICafObject());
		}

		SmartPtrCIntMessage messageImpl;
		messageImpl.CreateInstance();
		messageImpl->initialize(message->getPayload(),
				messageHeaders.getHeaders(), message->getHeaders());
		newMessage = messageImpl;
	}
	CAF_CM_EXIT;

	return newMessage;
}

SmartPtrIIntMessage CGuestAuthenticatorInstance::processErrorMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME("processErrorMessage");

	SmartPtrIIntMessage newMessage;

	try {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_SMARTPTR(message);

		CAF_CM_LOG_DEBUG_VA1("Called - %s", _id.c_str());

		newMessage = transformMessage(message);
	}
	CAF_CM_CATCH_ALL;
	CAF_CM_LOG_CRIT_CAFEXCEPTION;
	CAF_CM_CLEAREXCEPTION;

	return newMessage;
}

std::string CGuestAuthenticatorInstance::getSignedSamlToken(
		const SmartPtrCDynamicByteArray& payload) const {
	CAF_CM_FUNCNAME_VALIDATE("getSignedSamlToken");

	std::string signedSamlToken;

	CAF_CM_ENTER {
		CAF_CM_VALIDATE_SMARTPTR(payload);

		const SmartPtrCCafMessagePayload cafMessagePayload =
				CCafMessagePayload::create(payload);

		const SmartPtrCRequestHeaderDoc requestHeader =
				cafMessagePayload->getRequestHeader();
		const SmartPtrCAuthnAuthzCollectionDoc authnAuthzCollection =
				requestHeader->getAuthnAuthzCollection();
		CAF_CM_VALIDATE_SMARTPTR(authnAuthzCollection);
		CAF_CM_VALIDATE_STL(authnAuthzCollection->getAuthnAuthz());

		//TODO: Pass the other types (e.g. username / password) to VgAuth.
		//TODO: Use the sequence number to prioritize the types.
		//TODO: Consider creating a type for the string "SAML"
		for (std::deque<SmartPtrCAuthnAuthzDoc>::const_iterator authnAuthzIter =
				authnAuthzCollection->getAuthnAuthz().begin();
				authnAuthzIter != authnAuthzCollection->getAuthnAuthz().end();
				authnAuthzIter++) {
			const SmartPtrCAuthnAuthzDoc authnAuthz = *authnAuthzIter;
			if (authnAuthz->getType().compare("SAML") == 0) {
				signedSamlToken = authnAuthz->getValue();
			}
		}

		CAF_CM_VALIDATE_STRING(signedSamlToken);
	}
	CAF_CM_EXIT;

	return signedSamlToken;
}

std::string CGuestAuthenticatorInstance::findOptionalProperty(
	const std::string& propertyName,
	const IBean::Cprops& properties) const {
	CAF_CM_FUNCNAME_VALIDATE("findOptionalProperty");

	std::string rc;

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(propertyName);
		CAF_CM_VALIDATE_STL(properties);

		IBean::Cprops::const_iterator propertiesIter = properties.find(propertyName);
		if (propertiesIter != properties.end()) {
			rc = propertiesIter->second;
		}
	}
	CAF_CM_EXIT;

	return rc;
}

void CGuestAuthenticatorInstance::logUserInfo(
	const std::string& msg) const {
	CAF_CM_FUNCNAME_VALIDATE("logUserInfo");

	CAF_CM_ENTER {
		CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
		CAF_CM_VALIDATE_STRING(msg);

#ifndef WIN32
		CAF_CM_LOG_DEBUG_VA3("%s - UID: %d, GID: %d", msg.c_str(), ::geteuid(), ::getegid());
#endif
	}
	CAF_CM_EXIT;
}
