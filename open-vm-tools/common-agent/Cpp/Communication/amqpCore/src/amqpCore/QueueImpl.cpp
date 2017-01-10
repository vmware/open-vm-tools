/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpCore/Queue.h"
#include "amqpCore/QueueImpl.h"

using namespace Caf::AmqpIntegration;

QueueImpl::QueueImpl() :
	_durable(false),
	_exclusive(false),
	_autoDelete(false) {
}

QueueImpl::~QueueImpl() {
}

void QueueImpl::init(const std::string& name) {
	init(name, true, false, false);
}

void QueueImpl::init(
		const std::string& name,
		const bool durable) {
	init(name, durable, false, false);
}

void QueueImpl::init(
		const std::string& name,
		const bool durable,
		const bool exclusive,
		const bool autoDelete) {
	_name = name;
	_durable = durable;
	_exclusive = exclusive;
	_autoDelete = autoDelete;
}

std::string QueueImpl::getName() const {
	return _name;
}

bool QueueImpl::isDurable() const {
	return _durable;
}

bool QueueImpl::isExclusive() const {
	return _exclusive;
}

bool QueueImpl::isAutoDelete() const {
	return _autoDelete;
}

SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createQueue(
		const std::string& name) {
	SmartPtrQueueImpl queue;
	queue.CreateInstance();
	queue->init(name);
	return queue;
}

SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createQueue(
		const std::string& name,
		const bool durable) {
	SmartPtrQueueImpl queue;
	queue.CreateInstance();
	queue->init(name, durable);
	return queue;
}

SmartPtrQueue AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createQueue(
		const std::string& name,
		const bool durable,
		const bool exclusive,
		const bool autoDelete) {
	SmartPtrQueueImpl queue;
	queue.CreateInstance();
	queue->init(name, durable, exclusive, autoDelete);
	return queue;
}
