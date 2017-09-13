/*
 *  Created on: Jan 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (c) 2012 VMware, Inc.  All rights reserved.
 *  -- VMware Confidential
 */

#include "stdafx.h"
#include "CChannelInterceptorAdapter.h"

CChannelInterceptorAdapter::CChannelInterceptorAdapter() {
}

CChannelInterceptorAdapter::~CChannelInterceptorAdapter() {
}

SmartPtrIIntMessage& CChannelInterceptorAdapter::preSend(
		SmartPtrIIntMessage& message,
		SmartPtrIMessageChannel&) {
	return message;
}

void CChannelInterceptorAdapter::postSend(
		SmartPtrIIntMessage&,
		SmartPtrIMessageChannel&,
		bool) {
}

bool CChannelInterceptorAdapter::preReceive(
		SmartPtrIMessageChannel&) {
	return true;
}

SmartPtrIIntMessage& CChannelInterceptorAdapter::postReceive(
		SmartPtrIIntMessage& message,
		SmartPtrIMessageChannel&) {
	return message;
}
