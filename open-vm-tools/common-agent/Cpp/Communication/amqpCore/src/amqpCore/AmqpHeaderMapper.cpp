/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "amqpCore/AmqpHeaderMapper.h"

using namespace Caf::AmqpIntegration;

const std::string AmqpHeaderMapper::PREFIX = "amqp_";
const std::string AmqpHeaderMapper::APP_ID = AmqpHeaderMapper::PREFIX + "appId";
const std::string AmqpHeaderMapper::CLUSTER_ID = AmqpHeaderMapper::PREFIX + "clusterId";
const std::string AmqpHeaderMapper::CONTENT_ENCODING = AmqpHeaderMapper::PREFIX + "contentEncoding";
const std::string AmqpHeaderMapper::CONTENT_TYPE = AmqpHeaderMapper::PREFIX + "contentType";
const std::string AmqpHeaderMapper::CORRELATION_ID = AmqpHeaderMapper::PREFIX + "correlationId";
const std::string AmqpHeaderMapper::DELIVERY_MODE = AmqpHeaderMapper::PREFIX + "deliveryMode";
const std::string AmqpHeaderMapper::DELIVERY_TAG = AmqpHeaderMapper::PREFIX + "deliveryTag";
const std::string AmqpHeaderMapper::EXPIRATION = AmqpHeaderMapper::PREFIX + "expiration";
const std::string AmqpHeaderMapper::MESSAGE_COUNT = AmqpHeaderMapper::PREFIX + "messageCount";
const std::string AmqpHeaderMapper::MESSAGE_ID = AmqpHeaderMapper::PREFIX + "messageId";
const std::string AmqpHeaderMapper::RECEIVED_EXCHANGE = AmqpHeaderMapper::PREFIX + "receivedExchange";
const std::string AmqpHeaderMapper::RECEIVED_ROUTING_KEY = AmqpHeaderMapper::PREFIX + "receivedRoutingKey";
const std::string AmqpHeaderMapper::REDELIVERED = AmqpHeaderMapper::PREFIX + "redelivered";
const std::string AmqpHeaderMapper::REPLY_TO = AmqpHeaderMapper::PREFIX + "replyTo";
const std::string AmqpHeaderMapper::TIMESTAMP = AmqpHeaderMapper::PREFIX + "timestamp";
const std::string AmqpHeaderMapper::TYPE = AmqpHeaderMapper::PREFIX + "type";
const std::string AmqpHeaderMapper::USER_ID = AmqpHeaderMapper::PREFIX + "userId";

