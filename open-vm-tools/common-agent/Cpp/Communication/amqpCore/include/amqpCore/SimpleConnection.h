/*
 *  Created on: May 25, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_SIMPLECONNECTION_H_
#define AMQPINTEGRATIONCORE_SIMPLECONNECTION_H_

#include "amqpClient/api/Channel.h"
#include "amqpClient/api/Connection.h"
#include "amqpCore/Connection.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief A simple object that wraps a AmqpClient::Connection and exposes
 * it as an AmqpIntegration::Connection.
 */
class AMQPINTEGRATIONCORE_LINKAGE SimpleConnection : public Connection {
public:
	SimpleConnection();
	virtual ~SimpleConnection();

	/**
	 * @brief Initialize the object with the given AmqpClient::Connection
	 * @param delegate the wrapped AmqpClient::Connection
	 */
	void init(const AmqpClient::SmartPtrConnection& delegate);

public: // Connection
	AmqpClient::SmartPtrChannel createChannel();
	void close();
	bool isOpen();

private:
	AmqpClient::SmartPtrConnection _delegate;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(SimpleConnection);
};
CAF_DECLARE_SMART_POINTER(SimpleConnection);

}}

#endif
