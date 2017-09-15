/*
 *  Created on: Aug 16, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef ReplyToResolver_h
#define ReplyToResolver_h


#include "ICafObject.h"

#include "Integration/IIntMessage.h"

namespace Caf {
struct __declspec(novtable) ReplyToResolver : public ICafObject {
	CAF_DECL_UUID("4D306795-4475-4D2C-BEF0-0ADE28843BBC")

	virtual std::string cacheReplyTo(const SmartPtrIIntMessage& message) = 0;

	virtual std::string lookupReplyTo(const SmartPtrIIntMessage& message) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(ReplyToResolver);
}
#endif /* ReplyToResolver_h */
