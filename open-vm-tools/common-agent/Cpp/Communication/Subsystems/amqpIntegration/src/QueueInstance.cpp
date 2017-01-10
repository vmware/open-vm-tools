/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "amqpCore/Queue.h"
#include "Exception/CCafException.h"
#include "Common/IAppConfig.h"
#include "QueueInstance.h"

using namespace Caf::AmqpIntegration;

QueueInstance::QueueInstance() :
	CAF_CM_INIT("QueueInstance") {
}

QueueInstance::~QueueInstance() {
}

void QueueInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	_id = configSection->findRequiredAttribute("id");
	SmartPtrIAppConfig appConfig = getAppConfig();
	const std::string name = appConfig->resolveValue(configSection->findOptionalAttribute("name"));
	const std::string durable = configSection->findOptionalAttribute("durable");
	const std::string exclusive = configSection->findOptionalAttribute("exclusive");
	const std::string autoDelete = configSection->findOptionalAttribute("auto-delete");

	if (durable.length() && ((durable != "true") && (durable != "false"))) {
		CAF_CM_EXCEPTIONEX_VA2(
				InvalidArgumentException,
				0,
				"queue id (%s): 'durable' must be 'true' or 'false', not '%s'",
				_id.c_str(),
				durable.c_str());
	}
	if (exclusive.length() && ((exclusive != "true") && (exclusive != "false"))) {
		CAF_CM_EXCEPTIONEX_VA2(
				InvalidArgumentException,
				0,
				"queue id (%s): 'exclusive' must be 'true' or 'false', not '%s'",
				_id.c_str(),
				exclusive.c_str());
	}
	if (autoDelete.length() && ((autoDelete != "true") && (autoDelete != "false"))) {
		CAF_CM_EXCEPTIONEX_VA2(
				InvalidArgumentException,
				0,
				"queue id (%s): 'auto-delete' must be 'true' or 'false', not '%s'",
				_id.c_str(),
				autoDelete.c_str());
	}

	if (name.length()) {
		const bool durableFlag = durable.length() ? durable == "true" : false;
		const bool exclusiveFlag = exclusive.length() ? exclusive == "true" : false;
		const bool autoDeleteFlag = autoDelete.length() ? autoDelete == "true" : false;
		_queue = createQueue(name, durableFlag, exclusiveFlag, autoDeleteFlag);
	} else {
		if ((durable == "true") || (exclusive == "false") || (autoDelete == "false")) {
			CAF_CM_EXCEPTIONEX_VA1(
					InvalidArgumentException,
					0,
					"Anonymous queue (%s) cannot specify durable='true', exclusive='false', "
					"or auto-delete='false'",
					_id.c_str());
		}
		_queue = createQueue("", false, true, true);
	}
}

std::string QueueInstance::getId() const {
	return _id;
}

void QueueInstance::setQueueInternal(SmartPtrQueue queue) {
	_queue = queue;
}

std::string QueueInstance::getName() const {
	return _queue->getName();
}

bool QueueInstance::isDurable() const {
	return _queue->isDurable();
}

bool QueueInstance::isExclusive() const {
	return _queue->isExclusive();
}

bool QueueInstance::isAutoDelete() const {
	return _queue->isAutoDelete();
}
