/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef STDAFX_H_
#define STDAFX_H_

#ifdef WIN32
	#define INTEGRATIONCORE_LINKAGE __declspec(dllexport)
#else
	#define INTEGRATIONCORE_LINKAGE
#endif

#include <CommonDefines.h>
#include <IntegrationContracts.h>

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

#include "CMessagingTemplateHandler.h"
#include "CMessagingTemplate.h"


#endif /* STDAFX_H_ */
