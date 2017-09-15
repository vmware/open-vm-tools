/*
 *	 Author: bwilliams
 *  Created: Oct 22, 2010
 *
 *	Copyright (C) 2010-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef _IntegrationContracts_IIntMessage_h_
#define _IntegrationContracts_IIntMessage_h_


#include "ICafObject.h"

#include "IVariant.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"

namespace Caf {

/// TODO - describe interface
struct __declspec(novtable)
	IIntMessage : public ICafObject {
	CAF_DECL_UUID("c9abc77a-ebd1-4203-911f-1b37d9b17d8e")

	//
	// Routines dealing with the message
	//
	virtual UUID getMessageId() const = 0;

	virtual std::string getMessageIdStr() const = 0;

	//
	// Routines dealing with the payload
	//
	typedef std::map<std::string, std::pair<SmartPtrIVariant, SmartPtrICafObject> > CHeaders;
	CAF_DECLARE_SMART_POINTER(CHeaders);

	virtual SmartPtrCDynamicByteArray getPayload() const = 0;

	virtual std::string getPayloadStr() const = 0;

	//
	// Routines dealing with the headers
	//
	virtual SmartPtrCHeaders getHeaders() const = 0;

	virtual SmartPtrIVariant findOptionalHeader(
		const std::string& key) const = 0;

	virtual SmartPtrIVariant findRequiredHeader(
		const std::string& key) const = 0;

	virtual std::string findOptionalHeaderAsString(
		const std::string& key) const = 0;

	virtual std::string findRequiredHeaderAsString(
		const std::string& key) const = 0;

	virtual SmartPtrICafObject findOptionalObjectHeader(
		const std::string& key) const = 0;

	virtual SmartPtrICafObject findRequiredObjectHeader(
		const std::string& key) const = 0;
};

CAF_DECLARE_SMART_INTERFACE_POINTER(IIntMessage);

typedef std::deque<SmartPtrIIntMessage> CMessageCollection;
CAF_DECLARE_SMART_POINTER(CMessageCollection);

}

#endif // #ifndef _IntegrationContracts_IIntMessage_h_

