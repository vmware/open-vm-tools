/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Common/IAppContext.h"
#include "IBean.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"
#include "Exception/CCafException.h"
#include "CReplyToCacherInstance.h"

using namespace Caf;

CReplyToCacherInstance::CReplyToCacherInstance() :
	_isInitialized(false),
	CAF_CM_INIT("CReplyToCacherInstance") {
}

CReplyToCacherInstance::~CReplyToCacherInstance() {
}

void CReplyToCacherInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME_VALIDATE("initialize");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(configSection);
	_id = configSection->findRequiredAttribute("id");
	_replyToResolverId = configSection->findRequiredAttribute("reply-to-resolver");
	_isInitialized = true;
}

std::string CReplyToCacherInstance::getId() const {
	CAF_CM_FUNCNAME_VALIDATE("getId");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _id;
}

void CReplyToCacherInstance::wire(
	const SmartPtrIAppContext& appContext,
	const SmartPtrIChannelResolver& channelResolver) {
	CAF_CM_FUNCNAME("wire");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(appContext);
	SmartPtrIBean replyToResolverBean = appContext->getBean(_replyToResolverId);
	_replyToResolver.QueryInterface(replyToResolverBean, false);
	if (!_replyToResolver) {
		CAF_CM_EXCEPTIONEX_VA1(NoSuchInterfaceException, 0,
			"Bean '%s' is not a ReplyToResolver", _replyToResolverId.c_str());
	}
}

SmartPtrIIntMessage CReplyToCacherInstance::transformMessage(
	const SmartPtrIIntMessage& message) {
	CAF_CM_FUNCNAME_VALIDATE("transformMessage");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	CAF_CM_VALIDATE_INTERFACE(message);

	_replyToResolver->cacheReplyTo(message);

	return message;
}
