/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQCOMMAND_H_
#define AMQCOMMAND_H_



#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/CommandAssembler.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief This class represents an AMQP command
 * <p>
 * This class wraps up the method, content header and body information for an
 * incoming AMQP command.
 */
class AMQCommand {
public:
	AMQCommand();
	virtual ~AMQCommand();

	/**
	 * @brief Prepare the object for frame processing
	 */
	void init();

	/**
	 * @brief Process an AMQP frame
	 * @param frame amqp frame data
	 */
	bool handleFrame(const SmartPtrCAmqpFrame& frame);

	/**
	 * @brief Return the body if available
	 * @return the body's raw bytes if available or a <code><i>null</i></code> object
	 */
	SmartPtrCDynamicByteArray getContentBody();

	/**
	 * @brief Return the content header if available
	 * @return the IContentHeader if available or a
	 * <code><i>null</i></code> object
	 */
	SmartPtrIContentHeader getContentHeader();

	/**
	 * @brief Return the method
	 * @return the IMethod object interface representing the method.
	 * QueryInterface must be used to get to the actual method object
	 */
	SmartPtrIMethod getMethod();

private:
	SmartPtrCommandAssembler _assembler;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(AMQCommand);
};
CAF_DECLARE_SMART_POINTER(AMQCommand);

}}

#endif /* AMQCOMMAND_H_ */
