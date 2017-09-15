/*
 *  Created on: Jun 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpClient/api/Connection.h"
#include "amqpCore/CompositeConnectionListener.h"

using namespace Caf::AmqpIntegration;

CompositeConnectionListener::CompositeConnectionListener() :
	CAF_CM_INIT_LOG("CompositeConnectionListener") {
}

CompositeConnectionListener::~CompositeConnectionListener() {
}

void CompositeConnectionListener::setDelegates(
		const ListenerDeque& delegates) {
	_delegates = delegates;
}

void CompositeConnectionListener::addDelegate(
		const SmartPtrConnectionListener& delegate) {
	CAF_CM_FUNCNAME_VALIDATE("addDelegate");
	CAF_CM_VALIDATE_SMARTPTR(delegate);
	_delegates.push_back(delegate);
}

void CompositeConnectionListener::onCreate(
		const SmartPtrConnection& connection) {
	CAF_CM_FUNCNAME("onCreate");
	CAF_CM_VALIDATE_SMARTPTR(connection);
	for (TSmartIterator<ListenerDeque> delegate(_delegates);
			delegate;
			delegate++) {
		try {
			delegate->onCreate(connection);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
}

void CompositeConnectionListener::onClose(
		const SmartPtrConnection& connection) {
	CAF_CM_FUNCNAME("onClose");
	CAF_CM_VALIDATE_SMARTPTR(connection);
	for (TSmartIterator<ListenerDeque> delegate(_delegates);
			delegate;
			delegate++) {
		try {
			delegate->onClose(connection);
		}
		CAF_CM_CATCH_ALL;
		CAF_CM_LOG_CRIT_CAFEXCEPTION;
		CAF_CM_CLEAREXCEPTION;
	}
}
