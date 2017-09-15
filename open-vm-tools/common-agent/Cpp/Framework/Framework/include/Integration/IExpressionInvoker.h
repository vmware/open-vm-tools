/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IEXPRESSIONINVOKER_H_
#define _IntegrationContracts_IEXPRESSIONINVOKER_H_


#include "IVariant.h"
#include "Integration/IIntMessage.h"

namespace Caf {

struct __declspec(novtable) IExpressionInvoker : public ICafObject
{
	CAF_DECL_UUID("EF1DC19E-4DE0-416C-A7CB-D1695FF8D52A")

	virtual SmartPtrIVariant invokeExpression(
			const std::string& methodName,
			const Cdeqstr& methodParams,
			const SmartPtrIIntMessage& message) = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IExpressionInvoker);
}

#endif
