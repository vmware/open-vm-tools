/*
 *  Created on: Jun 7, 2012
 *      Author: mdonahue
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CVARIANT_H_
#define CVARIANT_H_


#include "IVariant.h"

namespace Caf {

/**
 * @brief A class that holds a GVariant* to manage its lifetime
 */
CAF_DECLARE_CLASS_AND_SMART_POINTER(CVariant);

class COMMONAGGREGATOR_LINKAGE CVariant : public IVariant {
public:
	CVariant();
	virtual ~CVariant();

	void set(GVariant *variant);

public: // IVariant
	GVariant *get() const;

	std::string toString() const;

	bool isString() const;
	bool isBool() const;
	bool isUint8() const;
	bool isInt16() const;
	bool isUint16() const;
	bool isInt32() const;
	bool isUint32() const;
	bool isInt64() const;
	bool isUint64() const;

public:
	static SmartPtrCVariant createString(const std::string& value);
	static SmartPtrCVariant createBool(const bool value);
	static SmartPtrCVariant createUint8(const uint8 value);
	static SmartPtrCVariant createInt16(const int16 value);
	static SmartPtrCVariant createUint16(const uint16 value);
	static SmartPtrCVariant createInt32(const int32 value);
	static SmartPtrCVariant createUint32(const uint32 value);
	static SmartPtrCVariant createInt64(const int64 value);
	static SmartPtrCVariant createUint64(const uint64 value);

private:
	bool isType(const GVariantType * varType) const;

private:
	GVariant *_variant;
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CVariant);
};

}

#endif /* CVARIANT_H_ */
