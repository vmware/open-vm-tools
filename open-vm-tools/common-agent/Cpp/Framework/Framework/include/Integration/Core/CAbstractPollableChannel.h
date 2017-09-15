/*
 *  Created on: Jan 26, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CABSTRACTPOLLABLECHANNEL_H_
#define CABSTRACTPOLLABLECHANNEL_H_

#include "Integration/Core/CAbstractMessageChannel.h"

#include "Integration/Dependencies/CPollerMetadata.h"
#include "Integration/IDocument.h"
#include "Integration/IIntMessage.h"

#include "Integration/IPollableChannel.h"

namespace Caf {

/**
 * Base class for pollable channels
 */
class INTEGRATIONCORE_LINKAGE CAbstractPollableChannel :
	public CAbstractMessageChannel,
	public IPollableChannel {
public:
	CAbstractPollableChannel();
	virtual ~CAbstractPollableChannel();

public:
	SmartPtrIIntMessage receive();
	SmartPtrIIntMessage receive(const int32 timeout);
	SmartPtrCPollerMetadata getPollerMetadata() const;

protected:
	/**
	 * Subclasses must implement this method. A non-negative timeout indicates
	 * how int32 to wait if the channel is empty (if the value is 0, it must
	 * return immediately with or without success). A negative timeout value
	 * indicates that the method should block until either a message is
	 * available or the blocking thread is interrupted.
	 */
	virtual SmartPtrIIntMessage doReceive(const int32 timeout) = 0;

	void setPollerMetadata(const SmartPtrCPollerMetadata& pollerMetadata);

	void setPollerMetadata(const SmartPtrIDocument& pollerDoc);

private:
	SmartPtrCPollerMetadata _pollerMetadata;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CAbstractPollableChannel);
};
}

#endif /* CABSTRACTPOLLABLECHANNEL_H_ */
