/*
 *  Created on: Jun 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "Integration/IDocument.h"
#include "amqpCore/Binding.h"
#include "Exception/CCafException.h"
#include "BindingInstance.h"

using namespace Caf::AmqpIntegration;

BindingInstance::BindingInstance() :
	CAF_CM_INIT("BindingInstance") {
	_id = CStringUtils::createRandomUuid();
}

BindingInstance::~BindingInstance() {
}

void BindingInstance::initialize(
	const IBean::Cargs& ctorArgs,
	const IBean::Cprops& properties,
	const SmartPtrIDocument& configSection) {
	CAF_CM_FUNCNAME("initialize");
	CAF_CM_EXCEPTIONEX_VA0(
			UnsupportedOperationException,
			0,
			"Binding init from xml not supported");
}

std::string BindingInstance::getId() const {
	return _id;
}

void BindingInstance::setBindingInternal(SmartPtrBinding binding) {
	_binding = binding;
}

std::string BindingInstance::getQueue() const {
	return _binding->getQueue();
}

std::string BindingInstance::getExchange() const {
	return _binding->getExchange();
}

std::string BindingInstance::getRoutingKey() const {
	return _binding->getRoutingKey();
}
