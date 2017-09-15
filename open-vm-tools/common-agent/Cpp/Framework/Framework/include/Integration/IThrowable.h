/*
 *	 Author: bwilliams
 *  Created: Oct. 25, 2011
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _FxContracts_IThrowable_h_
#define _FxContracts_IThrowable_h_


#include "ICafObject.h"

namespace Caf {

CAF_FORWARD_DECLARE_SMART_INTERFACE(IThrowable);

/// TODO - describe interface
struct __declspec(novtable)
IThrowable  : public ICafObject {
	CAF_DECL_UUID("5bced55d-06b7-4c4b-b805-90b51311dc9b")

	virtual std::string getExceptionClassName() const = 0;
	virtual std::string getMsg() const = 0;
	virtual std::string getClassName() const = 0;
	virtual std::string getFuncName() const = 0;
	virtual HRESULT getError() const = 0;
	virtual std::string getFullMsg() const = 0;
};

}

#endif


