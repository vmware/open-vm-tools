/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCafMessagePayload_h_
#define CCafMessagePayload_h_

#include <DocContracts.h>

#include "Integration/Caf/CCafMessagePayload.h"
#include "Doc/CafCoreTypesDoc/CRequestHeaderDoc.h"
#include "Doc/ResponseDoc/CEventKeyDoc.h"
#include "Doc/ResponseDoc/CManifestDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CCafMessagePayload);

class INTEGRATIONCAF_LINKAGE CCafMessagePayload {
public:
	static SmartPtrCCafMessagePayload create(
			const SmartPtrCDynamicByteArray& payload,
			const std::string& payloadType = std::string());

	static SmartPtrCCafMessagePayload createFromFile(
			const std::string& payloadFile,
			const std::string& payloadType = std::string());

	static SmartPtrCCafMessagePayload createFromStr(
			const std::string& payloadStr,
			const std::string& payloadType = std::string());

public:
	static SmartPtrCDynamicByteArray createBufferFromStr(
			const std::string& payloadStr);

	static SmartPtrCDynamicByteArray createBufferFromFile(
			const std::string& payloadFile);

public:
	static void saveToFile(
			const SmartPtrCDynamicByteArray& payload,
			const std::string& payloadPath);

	static std::string saveToStr(
			const SmartPtrCDynamicByteArray& payload);

public:
	CCafMessagePayload();
	virtual ~CCafMessagePayload();

public:
	std::string getPayloadStr() const;

	SmartPtrCDynamicByteArray getPayload() const;

public:
	std::string getVersion() const;

public:
	SmartPtrCRequestHeaderDoc getRequestHeader() const;

	SmartPtrCManifestDoc getManifest() const;

	std::deque<SmartPtrCEventKeyDoc> getEventKeyCollection() const;

private:
	void initialize(
			const SmartPtrCDynamicByteArray& payload,
			const std::string& payloadType = std::string(),
			const std::string& encoding = "xml");

private:
	bool _isInitialized;

	std::string _encoding;
	SmartPtrCDynamicByteArray _payload;
	std::string _payloadStr;
	SmartPtrCXmlElement _payloadXml;

private:
	CAF_CM_CREATE;
	CAF_CM_DECLARE_NOCOPY(CCafMessagePayload);
};

}

#endif // #ifndef CCafMessagePayload_h_
