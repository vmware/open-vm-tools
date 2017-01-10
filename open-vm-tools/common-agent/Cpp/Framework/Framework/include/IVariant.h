/*
 *  Created on: Jul 23, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _FxContracts_IVARIANT_H_
#define _FxContracts_IVARIANT_H_


#include "ICafObject.h"

namespace Caf {
struct __declspec(novtable) IVariant : public ICafObject {
	CAF_DECL_UUID("05AC7CB8-BBD4-4B3B-AB80-29002DD73747")

	virtual GVariant *get() const = 0;
	virtual std::string toString() const = 0;
	virtual bool isString() const = 0;
	virtual bool isBool() const = 0;
	virtual bool isUint8() const = 0;
	virtual bool isInt16() const = 0;
	virtual bool isUint16() const = 0;
	virtual bool isInt32() const = 0;
	virtual bool isUint32() const = 0;
	virtual bool isInt64() const = 0;
	virtual bool isUint64() const = 0;
};
CAF_DECLARE_SMART_INTERFACE_POINTER(IVariant);
}

#endif
