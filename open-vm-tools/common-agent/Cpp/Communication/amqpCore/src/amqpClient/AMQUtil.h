/*
 *  Created on: May 3, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef AMQUTIL_H_
#define AMQUTIL_H_


#include "amqpClient/api/amqpClient.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief A collection of helpers for dealing with AMQP and the underlying amqp library.
 */
namespace AMQUtil {
	/**
	 * @brief Check an AMQPStatus and throw an exception if the status is not AMQP_ERROR_OK
	 * @param status AMQPStatus returned from amqp library calls
	 * @param message Optional message text to add to thrown exception
	 */
	void checkAmqpStatus(
			const AMQPStatus status,
			const char* message = NULL);

	/**
	 * @brief Convert an amqp_bytes_t array to a std::string
	 * @param amqpBytes Raw string bytes
	 * @return conversion to string
	 */
	std::string amqpBytesToString(const amqp_bytes_t * const amqpBytes);

	/**
	 * @brief Convert an amqp_table_t to a smart Table object
	 * @param amqpTable Raw table bytes
	 * @return convertion to a smart Table object
	 */
	SmartPtrTable amqpApiTableToTableObj(const amqp_table_t * const amqpTable);

	/**
	 * @brief Convert a smart Table object to an amqp_table_t struct
	 * The caller is responsible for cleaning up the table by calling amqpFreeApiTable()
	 * @param table smart Table object source
	 * @param apiTable amqp_table_t output struct
	 */
	void amqpTableObjToApiTable(const SmartPtrTable& table, amqp_table_t& apiTable);

	/**
	 * @brief Clean up the memory used by a amqp_table_t
	 * @param table table to free
	 */
	void amqpFreeApiTable(amqp_table_t *table);
}

}}

#endif /* AMQUTIL_H_ */
