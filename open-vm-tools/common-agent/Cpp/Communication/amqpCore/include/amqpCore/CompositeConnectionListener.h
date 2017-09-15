/*
 *  Created on: Jun 2, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef COMPOSITECONNECTIONLISTENER_H_
#define COMPOSITECONNECTIONLISTENER_H_

#include "amqpClient/api/Connection.h"
#include "amqpCore/ConnectionListener.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Implementation of ConnectionListener that notifies multiple delegates
 */
class AMQPINTEGRATIONCORE_LINKAGE CompositeConnectionListener : public ConnectionListener {
public:
	CompositeConnectionListener();
	virtual ~CompositeConnectionListener();

	typedef std::deque<SmartPtrConnectionListener> ListenerDeque;

	/**
	 * @brief Set the delegate collection
	 * @param delegates the collection of ConnectionListener delegates
	 */
	void setDelegates(const ListenerDeque& delegates);

	/**
	 * @brief Add a delegate to the collection
	 * @param delegate the ConnectionListener delegate to add
	 */
	void addDelegate(const SmartPtrConnectionListener& delegate);

public: // ConnectionListener
	void onCreate(const SmartPtrConnection& connection);
	void onClose(const SmartPtrConnection& connection);

private:
	ListenerDeque _delegates;
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CompositeConnectionListener);
};
CAF_DECLARE_SMART_POINTER(CompositeConnectionListener);

}}

#endif /* COMPOSITECONNECTIONLISTENER_H_ */
