/*
 *  Created on: May 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CONNECTIONWEAKREFERENCE_H_
#define CONNECTIONWEAKREFERENCE_H_


#include "amqpClient/IConnectionInt.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief This class manages a weak reference to a IConnectionInt object.
 * <p>
 * The channel manager needs a reference to its connection.  The connection object
 * has a reference to the channel manager.  To break this reference cycle the channel
 * manager will be handed weak references to the connection.
 */
class ConnectionWeakReference : public IConnectionInt {
public:
	ConnectionWeakReference();
	virtual ~ConnectionWeakReference();

public:
	/**
	 * @brief Sets the weakly referenced object
	 * @param connection the weakly referenced object
	 */
	void setReference(IConnectionInt* connection);

	/**
	 * @brief Clears the weakly referenced object.
	 *
	 * Calls to any Connection method on this object will result in a thrown
	 * exception after this method has been called.
	 */
	void clearReference();

public: // IConnectionInt
	AMQPStatus amqpConnectionOpenChannel(SmartPtrCAmqpChannel& channel);

	void notifyChannelClosedByServer(const uint16 channelNumber);

	void channelCloseChannel(Channel *channel);

private:
	IConnectionInt* _connection;
	CAF_CM_CREATE;
	CAF_CM_CREATE_THREADSAFE;
	CAF_CM_DECLARE_NOCOPY(ConnectionWeakReference);
};
CAF_DECLARE_SMART_POINTER(ConnectionWeakReference);

}}

#endif /* CONNECTIONWEAKREFERENCE_H_ */
