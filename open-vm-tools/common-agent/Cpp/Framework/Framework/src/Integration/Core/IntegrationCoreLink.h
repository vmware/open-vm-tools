/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef INTEGRATIONCORELINK_H_
#define INTEGRATIONCORELINK_H_

#ifndef INTEGRATIONCORE_LINKAGE
    #ifdef WIN32
        #ifdef FRAMEWORK_BUILD
            #define INTEGRATIONCORE_LINKAGE __declspec(dllexport)
        #else
            #define INTEGRATIONCORE_LINKAGE __declspec(dllimport)
        #endif
    #else
        #define INTEGRATIONCORE_LINKAGE
    #endif
#endif

#include "FileHeaders.h"
#include "MessageHeaders.h"
#include "CIntMessageHeaders.h"
#include "CIntMessage.h"
#include "CDocument.h"
#include "CMessageHandler.h"
#include "CErrorHandler.h"
#include "CIntException.h"
#include "CChannelResolver.h"
#include "CIntegrationAppContext.h"
#include "CSimpleAsyncTaskExecutorState.h"
#include "CSimpleAsyncTaskExecutor.h"
#include "CSourcePollingChannelAdapter.h"
#include "CBroadcastingDispatcher.h"
#include "CUnicastingDispatcher.h"
#include "CChannelInterceptorAdapter.h"
#include "CAbstractMessageChannel.h"
#include "CAbstractPollableChannel.h"
#include "CMessagingTemplateHandler.h"
#include "CMessagingTemplate.h"
#include "CExpressionHandler.h"
#include "CAbstractMessageRouter.h"
#include "CMessageHeaderUtils.h"

#endif /* INTEGRATIONCORELINK_H_ */
