/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (c) 2012 VMware, Inc.  All rights reserved.
 *  -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORELINK_H_
#define AMQPINTEGRATIONCORELINK_H_

#ifndef AMQPINTEGRATIONCORE_LINKAGE
    #ifdef WIN32
        #ifdef AMQP_CLIENT
            #define AMQPINTEGRATIONCORE_LINKAGE __declspec(dllexport)
        #else
            #define AMQPINTEGRATIONCORE_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define AMQPINTEGRATIONCORE_LINKAGE
    #endif
#endif

#include "../amqpClient/api/amqpClient.h"
#include "Exchange.h"
#include "ExchangeImpl.h"
#include "Queue.h"
#include "QueueImpl.h"
#include "Binding.h"
#include "BindingImpl.h"
#include "ExchangeInternal.h"
#include "QueueInternal.h"
#include "BindingInternal.h"
#include "AbstractConnectionFactory.h"
#include "AmqpAdmin.h"
#include "AmqpHeaderMapper.h"
#include "AmqpIntegrationCoreDefines.h"
#include "AmqpIntegrationCoreFunc.h"
#include "AmqpIntegrationExceptions.h"
#include "AmqpMessageListenerSource.h"
#include "AmqpOutboundEndpoint.h"
#include "AmqpTemplate.h"
#include "AutoChannelClose.h"
#include "BlockingQueueConsumer.h"
#include "CachingConnectionFactory.h"
#include "Connection.h"
#include "ConnectionFactory.h"
#include "ConnectionListener.h"
#include "ConnectionProxy.h"
#include "DefaultAmqpHeaderMapper.h"
#include "HeaderUtils.h"
#include "MessageListener.h"
#include "RabbitAdmin.h"
#include "RabbitTemplate.h"
#include "SimpleConnection.h"
#include "SimpleMessageListenerContainer.h"

#endif /* AMQPINTEGRATIONCORELINK_H_ */
