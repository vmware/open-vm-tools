/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (c) 2012 VMware, Inc. All rights reserved.
 * -- VMware Confidential
 */

#include "stdafx.h"
#include "GetResponseImpl.h"

using namespace Caf::AmqpClient;

GetResponseImpl::GetResponseImpl() :
	_isInitialized(false),
	_messageCount(0),
	CAF_CM_INIT("GetResponseImpl") {
}

GetResponseImpl::~GetResponseImpl() {
}

void GetResponseImpl::init(
	const SmartPtrEnvelope& envelope,
	const AmqpContentHeaders::SmartPtrBasicProperties& properties,
	const SmartPtrCDynamicByteArray& body,
	const uint32 messageCount) {
	CAF_CM_FUNCNAME_VALIDATE("init");
	CAF_CM_PRECOND_ISNOTINITIALIZED(_isInitialized);
	_envelope = envelope;
	_properties = properties;
	_body = body;
	_messageCount = messageCount;
	_isInitialized = true;
}

SmartPtrEnvelope GetResponseImpl::getEnvelope() {
	CAF_CM_FUNCNAME_VALIDATE("getEnvelope");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _envelope;
}

AmqpContentHeaders::SmartPtrBasicProperties GetResponseImpl::getProperties() {
	CAF_CM_FUNCNAME_VALIDATE("getProperties");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _properties;
}

SmartPtrCDynamicByteArray GetResponseImpl::getBody() {
	CAF_CM_FUNCNAME_VALIDATE("getBody");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _body;
}

uint32 GetResponseImpl::getMessageCount() {
	CAF_CM_FUNCNAME_VALIDATE("getMessageCount");
	CAF_CM_PRECOND_ISINITIALIZED(_isInitialized);
	return _messageCount;
}
