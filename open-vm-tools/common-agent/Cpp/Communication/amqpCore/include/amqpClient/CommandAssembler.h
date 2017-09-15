/*
 *  Created on: May 9, 2012
 *      Author: mdonahue
 *
 *	Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef COMMANDASSEMBLER_H_
#define COMMANDASSEMBLER_H_


#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/amqpImpl/IContentHeader.h"
#include "amqpClient/amqpImpl/IMethod.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief A class that manages the assembly of AMQP frames into a complete AMQP method
 */
class CommandAssembler {
public:
	CommandAssembler();
	virtual ~CommandAssembler();

	/**
	 * @brief Initialize the object
	 */
	void init();

	/**
	 * @brief Process the AMQP frame
	 * @param frame AMQP frame
	 * @retval true all of the frames have been received and the AMQP method is complete
	 * @retval false more frames are required to complete the AMQP method
	 */
	bool handleFrame(const SmartPtrCAmqpFrame& frame);

	/**
	 * @brief Return the completion status of the method
	 * @retval true all frames have been received and the AMQP method is complete
	 * @retval false more frames are required to complete the AMQP method
	 */
	bool isComplete();

	/**
	 * @brief Return the Method
	 * @return the interface to the Method. QueryInterface to the appropriate
	 * derived type based on the class ID and method ID.
	 */
	SmartPtrIMethod getMethod();

	/**
	 * @brief Return the ContentHeader
	 * @return the interface to the ContentHeader. QueryInterface to the appropriate
	 * derived type based on the class ID.
	 */
	SmartPtrIContentHeader getContentHeader();

	/**
	 * @brief Return the method body
	 * @return the method body data as raw bytes
	 */
	SmartPtrCDynamicByteArray getContentBody();

private:
	typedef enum {
		EXPECTING_METHOD,
		EXPECTING_CONTENT_HEADER,
		EXPECTING_CONTENT_BODY,
		COMPLETE
	} CAState;

	typedef std::deque<SmartPtrCDynamicByteArray> CBodyCollection;

private:
	void consumeBodyFrame(const SmartPtrCAmqpFrame& frame);
	void consumeHeaderFrame(const SmartPtrCAmqpFrame& frame);
	void consumeMethodFrame(const SmartPtrCAmqpFrame& frame);
	void updateContentBodyState();
	void appendBodyFragment(const amqp_bytes_t * const fragment);
	SmartPtrCDynamicByteArray coalesceContentBody();

private:
	bool _isInitialized;
	CAState _state;
	SmartPtrIMethod _method;
	SmartPtrIContentHeader _contentHeader;
	uint32 _remainingBodyBytes;
	CBodyCollection _bodyCollection;
	uint32 _bodyLength;

	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CommandAssembler);
};
CAF_DECLARE_SMART_POINTER(CommandAssembler);

}}
#endif /* COMMANDASSEMBLER_H_ */
