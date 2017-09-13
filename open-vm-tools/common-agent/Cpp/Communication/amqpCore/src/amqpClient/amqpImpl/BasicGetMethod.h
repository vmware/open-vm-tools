/*
 *  Created on: May 16, 2012
 *      Author: mdonahue
 *
 * Copyright (c) 2012 VMware, Inc. All rights reserved.
 * -- VMware Confidential
 */

#ifndef BASICGETMETHOD_H_
#define BASICGETMETHOD_H_

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Implementation of AMQP basic.get
 */
class BasicGetMethod : public IServerMethod {
public:
	BasicGetMethod();
	virtual ~BasicGetMethod();

	/**
	 * @brief Initialize the method
	 * @param queue queue name
	 * @param noAck no acknowledgement (manual acknowledgement) fag
	 */
	void init(
		const std::string& queue,
		const bool noAck);

public: // IServerMethod
	std::string getMethodName() const;

	AMQPStatus send(const SmartPtrCAmqpChannel& channel);

private:
	bool _isInitialized;
	std::string _queue;
	bool _noAck;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(BasicGetMethod);
};
CAF_DECLARE_SMART_POINTER(BasicGetMethod);

}}

#endif /* BASICGETMETHOD_H_ */
