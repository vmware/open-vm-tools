/*
 *  Created on: Aug 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AmqpMessageListenerSource_h
#define AmqpMessageListenerSource_h

#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IIntMessage.h"
#include "amqpCore/AmqpHeaderMapper.h"
#include "amqpCore/MessageListener.h"
#include "Integration/Core/CAbstractPollableChannel.h"

namespace Caf { namespace AmqpIntegration {

class AMQPINTEGRATIONCORE_LINKAGE AmqpMessageListenerSource :
	public CAbstractPollableChannel,
	public MessageListener {
public:
	AmqpMessageListenerSource();
	virtual ~AmqpMessageListenerSource();

	void init(
			const SmartPtrAmqpHeaderMapper& headerMapper,
			const SmartPtrCPollerMetadata& pollerMetadata);

public: // MessageListener
	void onMessage(const SmartPtrIIntMessage& message);

private: // CAbstractPollableChannel
	bool doSend(
			const SmartPtrIIntMessage& message,
			int32 timeout);

	SmartPtrIIntMessage doReceive(const int32 timeout);

private:
	static void QueueItemDestroyFunc(gpointer data);

private:
	bool _isInitialized;
	std::string _id;
	GAsyncQueue *_messageQueue;
	SmartPtrAmqpHeaderMapper _headerMapper;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(AmqpMessageListenerSource);
};
CAF_DECLARE_SMART_POINTER(AmqpMessageListenerSource);

}}

#endif /* AmqpMessageListenerSource_h */
