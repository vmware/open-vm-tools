/*
 *  Created on: May 11, 2012
 *      Author: mdonahue
 *
 * Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef ICONTENTHEADER_H_
#define ICONTENTHEADER_H_

#include "amqpClient/CAmqpFrame.h"
#include "amqpClient/api/ContentHeader.h"

namespace Caf { namespace AmqpClient {

/**
 * @author mdonahue
 * @ingroup AmqpApiImpl
 * @remark LIBRARY IMPLEMENTATION - NOT PART OF THE PUBLIC API
 * @brief Base interface for AMQP content header implementation objects
 */
struct __declspec(novtable) IContentHeader : public ContentHeader {
	CAF_DECL_UUID("04068590-3083-446E-83AE-DACD90C0F470")

	/**
	 * @brief Initialize the header object from c-api properties data
	 * @param properties the properties data
	 */
	virtual void init(const SmartPtrCAmqpFrame& frame) = 0;

	/**
	 * @brief Return the expected method body data size
	 * @return the expected body size
	 */
	virtual uint64 getBodySize() = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IContentHeader);

}}

#endif /* ICONTENTHEADER_H_ */
