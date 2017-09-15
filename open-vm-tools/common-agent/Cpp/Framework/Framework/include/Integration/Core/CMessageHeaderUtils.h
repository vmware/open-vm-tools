/*
 *  Created on: Nov 19, 2014
 *      Author: bwilliams
 *
 *  Copyright (C) 2012-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 *
 */

#ifndef CMessageHeaderUtils_h_
#define CMessageHeaderUtils_h_


#include "Integration/IIntMessage.h"

namespace Caf {

class INTEGRATIONCORE_LINKAGE CMessageHeaderUtils {
public:
	static std::string getStringReq(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static std::string getStringOpt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint8 getUint8Req(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint8 getUint8Opt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint16 getUint16Req(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint16 getUint16Opt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint32 getUint32Req(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint32 getUint32Opt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint64 getUint64Req(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static uint64 getUint64Opt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static bool getBoolReq(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static bool getBoolOpt(
		const IIntMessage::SmartPtrCHeaders& headers,
		const std::string& tag);

	static void log(
		const IIntMessage::SmartPtrCHeaders& headers,
		const log4cpp::Priority::PriorityLevel priorityLevel = log4cpp::Priority::DEBUG);

private:
	CAF_CM_DECLARE_NOCREATE(CMessageHeaderUtils);
};

}

#endif
