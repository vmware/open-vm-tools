/*
 *	Author: bwilliams
 *  Created: Oct 20, 2011
 *
 *	Copyright (c) 2011 Vmware, Inc.  All rights reserved.
 *	-- VMware Confidential
 */

#ifndef IntegrationContracts_h_
#define IntegrationContracts_h_

#include <IntegrationContractsDefines.h>

// Dependencies
#include "Integration/Dependencies/CPollerMetadata.h"

// Interfaces
#include "Integration/IIntMessage.h"
#include "Integration/IThrowable.h"
#include "Integration/IErrorHandler.h"
#include "Integration/IErrorProcessor.h"
#include "Integration/IRunnable.h"
#include "Integration/ITaskExecutor.h"
#include "Integration/IDocument.h"
#include "Integration/IIntegrationObject.h"
#include "Integration/ILifecycle.h"
#include "Integration/IMessageChannel.h"
#include "Integration/IMessageHandler.h"
#include "Integration/IMessageProcessor.h"
#include "Integration/IMessageSplitter.h"
#include "Integration/IMessageDispatcher.h"
#include "Integration/IMessageProducer.h"
#include "Integration/IMessageRouter.h"
#include "Integration/ITransformer.h"
#include "Integration/IPollableChannel.h"
#include "Integration/ISubscribableChannel.h"
#include "Integration/IChannelResolver.h"
#include "Integration/IChannelInterceptor.h"
#include "Integration/IChannelInterceptorInstance.h"
#include "Integration/IChannelInterceptorSupport.h"
#include "Integration/IIntegrationComponentInstance.h"
#include "Integration/IIntegrationComponent.h"
#include "Integration/IIntegrationAppContext.h"
#include "Integration/IIntegrationAppContextAware.h"
#include "Integration/IExpressionInvoker.h"
#include "Integration/IPhased.h"
#include "Integration/ISmartLifecycle.h"

#endif // #ifndef IntegrationContracts_h_
