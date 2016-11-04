/*
 *	 Author: mdonahue
 *  Created: Jan 28, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef IAPPLICATIONCONTEXT_H_
#define IAPPLICATIONCONTEXT_H_



#include "ICafObject.h"

#include "IBean.h"

namespace Caf {

struct __declspec(novtable)
IAppContext : public ICafObject {
	CAF_DECL_UUID("f1d65e47-0f12-4301-861c-6a8c90099dae")

	// key=bean id
	typedef std::map<std::string, SmartPtrIBean> CBeans;
	CAF_DECLARE_SMART_POINTER(CBeans);

	virtual SmartPtrIBean getBean(const std::string& name) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IAppContext);
}

#endif /* IAPPLICATIONCONTEXT_H_ */
