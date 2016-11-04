/*
 *  Created on: Jun 12, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
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

#include "AmqpIntegrationCoreDefines.h"
#include "AmqpIntegrationCoreFunc.h"
#include "AmqpIntegrationExceptions.h"
#include "amqpCore/AmqpOutboundEndpoint.h"
#include "AutoChannelClose.h"
#include "HeaderUtils.h"

#endif /* AMQPINTEGRATIONCORELINK_H_ */
