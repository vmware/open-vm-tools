/*
 *  Created on: May 22, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef BASICQOSMETHOD_H_
#define BASICQOSMETHOD_H_


#include "amqpClient/amqpImpl/IServerMethod.h"

#include "amqpClient/CAmqpChannel.h"

namespace Caf { namespace AmqpClient {
/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.qos
 */
class BasicQosMethod : public IServerMethod {
public:
	BasicQosMethod();
	virtual ~BasicQosMethod();

	/**
	 * @brief Initialize the method
	 * @param prefetchSize prefetch window in octets
	 * @param prefetchCount prefetch windows in messages
	 * @param global apply to entire connection
	 */
	void init(
		const uint32 prefetchSize,
		const uint16 prefetchCount,
		const bool global);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	uint32 _prefetchSize;
	uint16 _prefetchCount;
	bool _global;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicQosMethod);

};
CAF_DECLARE_SMART_POINTER(BasicQosMethod);

}}

#endif /* BASICQOSMETHOD_H_ */
