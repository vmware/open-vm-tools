/*
 *  Created on: Jun 14, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"

#include "amqpCore/Exchange.h"
#include "amqpCore/ExchangeImpl.h"

using namespace Caf::AmqpIntegration;

const char *ExchangeTypes::DIRECT = "direct";
const char *ExchangeTypes::TOPIC = "topic";
const char *ExchangeTypes::HEADERS = "headers";
const char *ExchangeTypes::FANOUT = "fanout";

AbstractExchange::AbstractExchange() :
	_isDurable(true) {
}

AbstractExchange::~AbstractExchange() {
}

void AbstractExchange::init(
		const std::string& name,
		const bool isDurable) {
	_name = name;
	_isDurable = isDurable;
}

std::string AbstractExchange::getName() const {
	return _name;
}

bool AbstractExchange::isDurable() const {
	return _isDurable;
}

DirectExchange::DirectExchange() {
}

void DirectExchange::init(
		const std::string name,
		const bool durable) {
	AbstractExchange::init(name, durable);
}

std::string DirectExchange::getType() const {
	return ExchangeTypes::DIRECT;
}

SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createDirectExchange(
		const std::string& name,
		const bool durable) {
	SmartPtrDirectExchange exchange;
	exchange.CreateInstance();
	exchange->init(name, durable);
	return exchange;
}

TopicExchange::TopicExchange() {
}

void TopicExchange::init(
		const std::string name,
		const bool durable) {
	AbstractExchange::init(name, durable);
}

std::string TopicExchange::getType() const {
	return ExchangeTypes::TOPIC;
}

SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createTopicExchange(
		const std::string& name,
		const bool durable) {
	SmartPtrTopicExchange exchange;
	exchange.CreateInstance();
	exchange->init(name, durable);
	return exchange;
}

HeadersExchange::HeadersExchange() {
}

void HeadersExchange::init(
		const std::string name,
		const bool durable) {
	AbstractExchange::init(name, durable);
}

std::string HeadersExchange::getType() const {
	return ExchangeTypes::HEADERS;
}

SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createHeadersExchange(
		const std::string& name,
		const bool durable) {
	SmartPtrHeadersExchange exchange;
	exchange.CreateInstance();
	exchange->init(name, durable);
	return exchange;
}

FanoutExchange::FanoutExchange() {
}

void FanoutExchange::init(
		const std::string name,
		const bool durable) {
	AbstractExchange::init(name, durable);
}

std::string FanoutExchange::getType() const {
	return ExchangeTypes::FANOUT;
}

SmartPtrExchange AMQPINTEGRATIONCORE_LINKAGE Caf::AmqpIntegration::createFanoutExchange(
		const std::string& name,
		const bool durable) {
	SmartPtrFanoutExchange exchange;
	exchange.CreateInstance();
	exchange->init(name, durable);
	return exchange;
}
