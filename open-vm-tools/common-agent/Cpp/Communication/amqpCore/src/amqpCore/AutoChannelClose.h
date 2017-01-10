/*
 *  Created on: Jun 15, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_AUTOCHANNELCLOSE_H_
#define AMQPINTEGRATIONCORE_AUTOCHANNELCLOSE_H_


#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief A helper class to automatically close a channel
 * <p>
 * This class is helpful to mimic try/catch/finally logic in the code.
 * Simply declare an instance on the stack, initialized with the channel.
 * When the instance goes out of scope it will close the channel.
 */
class AMQPINTEGRATIONCORE_LINKAGE AutoChannelClose {
public:
	/**
	 * @brief Construct the instance with the given channel
	 * @param channel the channel to auto-close
	 */
	AutoChannelClose(AmqpClient::SmartPtrChannel channel);
	~AutoChannelClose();

private:
	AmqpClient::SmartPtrChannel _channel;
};

}}

#endif
