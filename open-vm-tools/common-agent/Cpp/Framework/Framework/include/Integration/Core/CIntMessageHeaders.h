/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CIntMessageHeaders_h_
#define CIntMessageHeaders_h_

#include "Integration/Core/CIntMessageHeaders.h"

#include "ICafObject.h"
#include "IVariant.h"
#include "Integration/IIntMessage.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CIntMessageHeaders {
public:
	CIntMessageHeaders();
	virtual ~CIntMessageHeaders();

public:
	IIntMessage::SmartPtrCHeaders getHeaders() const;

	void clear();

	void insertString(
		const std::string& key,
		const std::string& value);

	void insertStringOpt(
		const std::string& key,
		const std::string& value);

	void insertInt64(
		const std::string& key,
		const int64& value);

	void insertUint64(
		const std::string& key,
		const uint64& value);

	void insertInt32(
		const std::string& key,
		const int32& value);

	void insertUint32(
		const std::string& key,
		const uint32& value);

	void insertInt16(
		const std::string& key,
		const int16& value);

	void insertUint16(
		const std::string& key,
		const uint16& value);

	void insertUint8(
		const std::string& key,
		const uint8& value);

	void insertBool(
		const std::string& key,
		const bool& value);

	void insertVariant(
		const std::string& key,
		const SmartPtrIVariant& variant);

	void insertObject(
		const std::string& key,
		const SmartPtrICafObject& cafObject);

private:
	IIntMessage::SmartPtrCHeaders _headers;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CIntMessageHeaders);
};

CAF_DECLARE_SMART_POINTER(CIntMessageHeaders);

}

#endif // #ifndef _IntegrationContracts_CIntMessageHeaders_h_
