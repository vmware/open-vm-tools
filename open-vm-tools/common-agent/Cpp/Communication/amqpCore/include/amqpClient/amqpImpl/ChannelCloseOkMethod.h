/*
 *  Created on: May 17, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHANNELCLOSEOKMETHOD_H_
#define CHANNELCLOSEOKMETHOD_H_

#include "amqpClient/amqpImpl/IServerMethod.h"
#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementatin of AMQP channel.close-ok
 */
class ChannelCloseOkMethod :public IServerMethod {
public:
	ChannelCloseOkMethod();
	virtual ~ChannelCloseOkMethod();

	/**
	 * Initialize the method
	 */
	void init();

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ChannelCloseOkMethod);
};
CAF_DECLARE_SMART_POINTER(ChannelCloseOkMethod);

}}

#endif /* CHANNELCLOSEOKMETHOD_H_ */
