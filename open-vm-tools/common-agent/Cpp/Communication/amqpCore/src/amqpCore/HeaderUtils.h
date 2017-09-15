/*
 *  Created on: Jun 6, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQPINTEGRATIONCORE_HEADERUTILS_H_
#define AMQPINTEGRATIONCORE_HEADERUTILS_H_

#include "Common/CVariant.h"
#include "Integration/IIntMessage.h"

namespace Caf { namespace AmqpIntegration {

/**
 * @brief A collection of helper functions
 */
class AMQPINTEGRATIONCORE_LINKAGE HeaderUtils {
public:
	/**
	 * @brief Extract a header value as text
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant String
	 */
	static SmartPtrCVariant getHeaderString(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);

	/**
	 * @brief Extract a header value as an unsigned 8-bit integer
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant Uint8
	 */
	static SmartPtrCVariant getHeaderUint8(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);

	/**
	 * @brief Extract a header value as an unsigned 16-bit integer
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant Uint16
	 */
	static SmartPtrCVariant getHeaderUint16(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);

	/**
	 * @brief Extract a header value as an unsigned 32-bit integer
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant Uint32
	 */
	static SmartPtrCVariant getHeaderUint32(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);

	/**
	 * @brief Extract a header value as an unsigned 64-bit integer
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant Uint64
	 */
	static SmartPtrCVariant getHeaderUint64(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);

	/**
	 * @brief Extract a header value as a boolean
	 * @param headers message headers
	 * @param tag value name
	 * @return the value as a variant Bool
	 */
	static SmartPtrCVariant getHeaderBool(
			const IIntMessage::SmartPtrCHeaders& headers,
			const std::string& tag);
};

}}

#endif /* AMQPINTEGRATIONCORE_HEADERUTILS_H_ */
