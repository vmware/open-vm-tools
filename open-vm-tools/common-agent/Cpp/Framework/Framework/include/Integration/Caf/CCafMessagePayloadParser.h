/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCafMessagePayloadParser_h_
#define CCafMessagePayloadParser_h_


#include "Doc/CafInstallRequestDoc/CInstallProviderJobDoc.h"
#include "Doc/CafInstallRequestDoc/CInstallRequestDoc.h"
#include "Doc/CafInstallRequestDoc/CUninstallProviderJobDoc.h"
#include "Doc/MgmtRequestDoc/CMgmtRequestDoc.h"
#include "Doc/PayloadEnvelopeDoc/CPayloadEnvelopeDoc.h"
#include "Doc/ProviderInfraDoc/CProviderRegDoc.h"
#include "Doc/ProviderRequestDoc/CProviderRequestDoc.h"
#include "Memory/DynamicArray/DynamicArrayInc.h"
#include "Xml/XmlUtils/CXmlElement.h"

namespace Caf {

CAF_DECLARE_CLASS_AND_SMART_POINTER(CCafMessagePayloadParser);

class INTEGRATIONCAF_LINKAGE CCafMessagePayloadParser {
public:
	static SmartPtrCPayloadEnvelopeDoc getPayloadEnvelope(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCInstallProviderJobDoc getInstallProviderJob(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCUninstallProviderJobDoc getUninstallProviderJob(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCProviderRequestDoc getProviderRequest(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCProviderRegDoc getProviderReg(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCInstallRequestDoc getInstallRequest(
			const SmartPtrCDynamicByteArray& payload);

	static SmartPtrCMgmtRequestDoc getMgmtRequest(
			const SmartPtrCDynamicByteArray& payload);

private:
	static SmartPtrCXmlElement bufferToXml(
			const SmartPtrCDynamicByteArray& payload,
			const std::string& payloadType = std::string());

	static std::string bufferToStr(
			const SmartPtrCDynamicByteArray& payload);

private:
	CAF_CM_DECLARE_NOCREATE(CCafMessagePayloadParser);
};

}

#endif // #ifndef CCafMessagePayloadParser_h_
