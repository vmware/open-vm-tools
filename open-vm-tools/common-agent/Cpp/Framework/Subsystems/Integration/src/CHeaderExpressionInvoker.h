/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CHEADEREXPRESSIONINVOKER_H_
#define CHEADEREXPRESSIONINVOKER_H_


#include "IBean.h"

#include "IVariant.h"
#include "IntegrationSubsys.h"
#include "Integration/IIntMessage.h"
#include "Integration/IExpressionInvoker.h"

namespace Caf {

class CHeaderExpressionInvoker :
	public TCafSubSystemObjectRoot<CHeaderExpressionInvoker>,
	public IBean,
	public IExpressionInvoker {
public:
	CHeaderExpressionInvoker();
	virtual ~CHeaderExpressionInvoker();

	CAF_DECLARE_OBJECT_IDENTIFIER(_sObjIdHeaderExpressionInvoker)

	CAF_BEGIN_INTERFACE_MAP(CHeaderExpressionInvoker)
		CAF_INTERFACE_ENTRY(IBean)
		CAF_INTERFACE_ENTRY(IExpressionInvoker)
	CAF_END_INTERFACE_MAP()

public: // IBean
	void initializeBean(
			const IBean::Cargs& ctorArgs,
			const IBean::Cprops& properties);

	void terminateBean();

public: // IExpressionInvoker
	SmartPtrIVariant invokeExpression(
			const std::string& methodName,
			const Cdeqstr& methodParams,
			const SmartPtrIIntMessage& message);

private:
	bool _isInitialized;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CHeaderExpressionInvoker);
};

}

#endif /* CHEADEREXPRESSIONINVOKER_H_ */
