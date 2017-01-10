/*
 *  Created on: May 17, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHANNELCLOSEMETHOD_H_
#define CHANNELCLOSEMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementaiton of AMQP channel.close
 */
class ChannelCloseMethod :
	public TMethodImpl<ChannelCloseMethod>,
	public AmqpMethods::Channel::Close {
	METHOD_DECL(
		AmqpMethods::Channel::Close,
		AMQP_CHANNEL_CLOSE_METHOD,
		"channel.close",
		false)

public:
	ChannelCloseMethod();
	virtual ~ChannelCloseMethod();

public: // IMethod
	void init(const amqp_method_t * const method);

public: // AmqpMethods::Channel::Close
	uint16 getReplyCode();
	std::string getReplyText();
	uint16 getClassId();
	uint16 getMethodId();

private:
	uint16 _replyCode;
	std::string _replyText;
	uint16 _classId;
	uint16 _methodId;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(ChannelCloseMethod);
};
CAF_DECLARE_SMART_QI_POINTER(ChannelCloseMethod);

}}

#endif /* CHANNELCLOSEMETHOD_H_ */
