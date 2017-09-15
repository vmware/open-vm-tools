/*
 *  Created on: May 29, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHANNELPROXY_H_
#define CHANNELPROXY_H_


#include "amqpClient/api/Channel.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @ingroup IntObjImpl
 * @brief Interface on objects used to proxy channel objects for the various
 * connection objects.
 */
struct __declspec(novtable) ChannelProxy : public AmqpClient::Channel {

	/**
	 * @brief Return the proxied Channel object
	 * @return the proxied channel
	 */
	virtual AmqpClient::SmartPtrChannel getTargetChannel() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ChannelProxy);

}}

#endif /* CHANNELPROXY_H_ */
